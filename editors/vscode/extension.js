'use strict';

const vscode = require('vscode');
const cp     = require('child_process');
const path   = require('path');
const fs     = require('fs');

function fluxcPath() {
    const configured = vscode.workspace.getConfiguration('flux').get('compilerPath');
    if (configured && configured !== 'fluxc') return configured;
    const bundled = path.join(__dirname, 'fluxc.exe');
    if (fs.existsSync(bundled)) return bundled;
    return 'fluxc';
}

// ── Manual LSP client ─────────────────────────────────────────────────────────

class LspClient {
    constructor(exePath) {
        this._exe = exePath;
        this._proc = null;
        this._buf = Buffer.alloc(0);
        this._id = 1;
        this._pending = new Map(); // id → {resolve, reject, timer}
    }

    start() {
        this._proc = cp.spawn(this._exe, ['--lsp'], {
            stdio: ['pipe', 'pipe', 'ignore']
        });

        this._proc.stdout.on('data', chunk => {
            this._buf = Buffer.concat([this._buf, chunk]);
            this._drain();
        });

        this._proc.on('exit', () => {
            for (const [, {reject, timer}] of this._pending) {
                clearTimeout(timer);
                reject(new Error('LSP server exited'));
            }
            this._pending.clear();
        });

        // initialize handshake
        this._notify('initialize', {
            processId: process.pid,
            rootUri: null,
            capabilities: {
                textDocument: {
                    synchronization: { openClose: true, change: 1 },
                    completion:      { dynamicRegistration: false },
                    hover:           { dynamicRegistration: false }
                }
            }
        });
    }

    _drain() {
        while (true) {
            const s = this._buf.toString('utf8');
            const m = s.match(/^Content-Length: (\d+)\r\n\r\n/);
            if (!m) break;
            const headerLen = Buffer.byteLength(m[0], 'utf8');
            const bodyLen   = parseInt(m[1]);
            if (this._buf.length < headerLen + bodyLen) break;
            const body = this._buf.slice(headerLen, headerLen + bodyLen).toString('utf8');
            this._buf = this._buf.slice(headerLen + bodyLen);
            this._onMessage(body);
        }
    }

    _onMessage(raw) {
        let msg;
        try { msg = JSON.parse(raw); } catch { return; }
        if (msg.id !== undefined && this._pending.has(msg.id)) {
            const { resolve, reject, timer } = this._pending.get(msg.id);
            this._pending.delete(msg.id);
            clearTimeout(timer);
            if (msg.error) reject(new Error(msg.error.message || 'LSP error'));
            else resolve(msg.result);
        }
    }

    _write(msg) {
        if (!this._proc || this._proc.killed) return;
        const body = JSON.stringify(msg);
        const header = `Content-Length: ${Buffer.byteLength(body, 'utf8')}\r\n\r\n`;
        this._proc.stdin.write(header + body, 'utf8');
    }

    _request(method, params) {
        const id = this._id++;
        this._write({ jsonrpc: '2.0', id, method, params });
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this._pending.delete(id);
                reject(new Error(`LSP timeout: ${method}`));
            }, 5000);
            this._pending.set(id, { resolve, reject, timer });
        });
    }

    _notify(method, params) {
        this._write({ jsonrpc: '2.0', method, params });
    }

    // Public API
    open(uri, text) {
        this._notify('textDocument/didOpen', {
            textDocument: { uri, languageId: 'flux', version: 1, text }
        });
    }
    change(uri, version, text) {
        this._notify('textDocument/didChange', {
            textDocument: { uri, version },
            contentChanges: [{ text }]
        });
    }
    close(uri) {
        this._notify('textDocument/didClose', { textDocument: { uri } });
    }
    complete(uri, line, character) {
        return this._request('textDocument/completion', {
            textDocument: { uri },
            position: { line, character }
        });
    }
    hover(uri, line, character) {
        return this._request('textDocument/hover', {
            textDocument: { uri },
            position: { line, character }
        });
    }
    definition(uri, line, character) {
        return this._request('textDocument/definition', {
            textDocument: { uri },
            position: { line, character }
        });
    }
    stop() {
        if (this._proc) {
            try { this._write({ jsonrpc: '2.0', method: 'exit', params: {} }); } catch {}
            this._proc.kill();
            this._proc = null;
        }
    }
}

// ── Activation ────────────────────────────────────────────────────────────────

function activate(context) {
    const client = new LspClient(fluxcPath());
    client.start();

    // Sync already-open documents
    for (const doc of vscode.workspace.textDocuments)
        if (doc.languageId === 'flux')
            client.open(doc.uri.toString(), doc.getText());

    context.subscriptions.push(
        vscode.workspace.onDidOpenTextDocument(doc => {
            if (doc.languageId === 'flux')
                client.open(doc.uri.toString(), doc.getText());
        }),
        vscode.workspace.onDidChangeTextDocument(e => {
            if (e.document.languageId === 'flux')
                client.change(e.document.uri.toString(), e.document.version, e.document.getText());
        }),
        vscode.workspace.onDidCloseTextDocument(doc => {
            if (doc.languageId === 'flux')
                client.close(doc.uri.toString());
        }),

        // Completion
        vscode.languages.registerCompletionItemProvider(
            { language: 'flux', scheme: 'file' },
            {
                async provideCompletionItems(document, position) {
                    try {
                        const r = await client.complete(
                            document.uri.toString(),
                            position.line,
                            position.character
                        );
                        if (!r || !Array.isArray(r.items)) return [];
                        return r.items.map(item => {
                            // LSP kind (1-based) → VS Code kind (0-based)
                            const kind = typeof item.kind === 'number' ? item.kind - 1 : 0;
                            const ci = new vscode.CompletionItem(item.label, kind);
                            if (item.detail) ci.detail = item.detail;
                            return ci;
                        });
                    } catch { return []; }
                }
            },
            '.'
        ),

        // Hover
        vscode.languages.registerHoverProvider(
            { language: 'flux', scheme: 'file' },
            {
                async provideHover(document, position) {
                    try {
                        const r = await client.hover(
                            document.uri.toString(),
                            position.line,
                            position.character
                        );
                        if (!r) return null;
                        const md = typeof r.contents === 'string'
                            ? r.contents
                            : (r.contents && r.contents.value) || '';
                        return md ? new vscode.Hover(new vscode.MarkdownString(md)) : null;
                    } catch { return null; }
                }
            }
        ),

        // Go to definition (Ctrl+Click)
        vscode.languages.registerDefinitionProvider(
            { language: 'flux', scheme: 'file' },
            {
                async provideDefinition(document, position) {
                    try {
                        const r = await client.definition(
                            document.uri.toString(),
                            position.line,
                            position.character
                        );
                        if (!r || !r.uri) return null;
                        const uri   = vscode.Uri.parse(r.uri);
                        const start = new vscode.Position(r.range.start.line, r.range.start.character);
                        const end   = new vscode.Position(r.range.end.line,   r.range.end.character);
                        return new vscode.Location(uri, new vscode.Range(start, end));
                    } catch { return null; }
                }
            }
        ),

        { dispose: () => client.stop() }
    );
}

function deactivate() {}

module.exports = { activate, deactivate };
