## Импорт модулей

```flux
#import "relative/path.flx"   // файл на диске
#import <io>                   // встроенный stdlib-модуль
#import <math>
#import <string>
```

`#import` — директива препроцессора. Она вставляет содержимое указанного файла на место директивы ещё **до** парсинга. Циклические импорты обнаруживаются и игнорируются с предупреждением.

---

## Функции

```flux
fnc имя(параметр: Тип, ...) -> ВозврТип {
    // тело
}
```

### Примеры

```flux
// Простая функция
fnc add(a: int32, b: int32) -> int32 {
    return a + b;
}

// Без возвращаемого значения (unit-тип)
fnc greet(name: string) -> () {
    println("Hello, ", name);
}

// Публичная функция (видима при #import)
pub fnc square(x: double) -> double {
    return x * x;
}

// Функция без тела — реализована в C++ runtime
extern fnc sqrt(x: double) -> double;
```

### Точка входа `main`

Допустимые сигнатуры:
```flux
fnc main() -> int32 { ... }
fnc main() -> Result<int32, string> { ... }
fnc main(argc: int32, argv: &&str) -> int32 { ... }
```

---

## Переменные

```flux
let имя: Тип = выражение;   // с явным типом
let имя = выражение;         // вывод типа
```

```flux
let x: int32 = 42;
let y = 3.14;          // double (по умолчанию для float-литералов)
let s: string = "hello";
let flag = true;
```

Все переменные изменяемы (нет `const`/`mut` на данный момент).

---

## Типы данных

### Примитивные числовые типы

| Flux | C++ | Описание |
|------|-----|----------|
| `bool` | `bool` | Булев |
| `int8` | `int8_t` | 8-битное знаковое |
| `int16` | `int16_t` | 16-битное знаковое |
| `int32` | `int32_t` | 32-битное знаковое |
| `int64` | `int64_t` | 64-битное знаковое |
| `int128` | `__int128` | 128-битное знаковое |
| `uint8` | `uint8_t` | 8-битное беззнаковое |
| `uint16` | `uint16_t` | 16-битное беззнаковое |
| `uint32` | `uint32_t` | 32-битное беззнаковое |
| `uint64` | `uint64_t` | 64-битное беззнаковое |
| `uint128` | `unsigned __int128` | 128-битное беззнаковое |
| `float` | `float` | 32-битное вещественное |
| `double` | `double` | 64-битное вещественное |
| `isize_t` | `ptrdiff_t` | Знаковый размер |
| `usize_t` | `size_t` | Беззнаковый размер |

### Строковые типы

| Flux | C++ | Описание |
|------|-----|----------|
| `string` | `std::string` | Владеющая строка |
| `&str` / `str` | `std::string_view` | Ссылка на строку |

`"литерал"` имеет тип `&str`, но может быть присвоен переменной `string` — семантический анализатор считает их совместимыми.

### Составные типы

```flux
let arr: int32[] = [1, 2, 3];        // массив (std::vector<int32_t>)
let opt: Option<int32> = Some(42);   // опциональный (std::optional<int32_t>)
let res: Result<int32, string> = Ok(0); // результат (Result<int32_t, std::string>)
```

### Ссылки и указатели

```flux
let ptr: &int32 = &x;   // ссылка (C++ reference)
let val = *ptr;          // разыменование
```

---

## Управляющие конструкции

### if / else

```flux
if (условие) {
    // ...
} else if (другое_условие) {
    // ...
} else {
    // ...
}
```

### while

```flux
while (условие) {
    // ...
}
```

### for

```flux
for (let i: int32 = 0; i < 10; i++) {
    println(i);
}
```

### break / continue

```flux
while (true) {
    if x > 10 { break; }
    x++;
}
```

---

## Структуры

```flux
struct Point {
    pub x: int32,
    pub y: int32,
}

// Создание
let p = Point { x: 3, y: 4 };

// Доступ к полям
println(p.x);
```

Поля без `pub` недоступны снаружи структуры.

### impl — методы структуры

```flux
impl Point {
    pub fnc distance(self) -> double {
        let fx: double = self.x;
        let fy: double = self.y;
        return sqrt(fx * fx + fy * fy);
    }

    // Статический метод (без self)
    pub fnc origin() -> Point {
        return Point { x: 0, y: 0 };
    }
}

// Использование
let p = Point { x: 3, y: 4 };
let d = p.distance();           // вызов метода
let o = Point.origin();         // вызов статического метода
```

---

## Классы

`class` — сокращённая запись `struct` + `impl` вместе:

```flux
class Counter {
    pub value: int32,

    pub fnc new(start: int32) -> Counter {
        return Counter { value: start };
    }

    pub fnc increment(self) -> () {
        self.value++;
    }

    pub fnc get(self) -> int32 {
        return self.value;
    }
}

let c = Counter.new(0);
c.increment();
println(c.get());   // 1
```

---

## Операторы

### Арифметические
`+`, `-`, `*`, `/`, `%`

### Сравнение
`==`, `!=`, `<`, `>`, `<=`, `>=`

### Логические
`&&`, `||`, `!`

### Побитовые
`&`, `|`, `^`, `~`, `<<`, `>>`

### Присваивание
`=`, `+=`, `-=`, `*=`, `/=`, `%=`

### Инкремент/декремент
`++x`, `--x`, `x++`, `x--`

---

## match — сопоставление с образцом

```flux
let x: int32 = 2;

match x {
    1 => { println("one"); }
    2 => { println("two"); }
    _ => { println("other"); }
}
```

### match с Result

```flux
let result: Result<int32, string> = Ok(42);

match result {
    Ok(val) => { println("Got: ", val); }
    Err(e)  => { println("Error: ", e); }
}
```

---

## Встроенные функции вывода

Доступны без `#import` в любой программе:

```flux
println("значение = ", x);    // вывод с переводом строки
print("без переноса");         // вывод без '\n'
eprintln("в stderr");          // вывод в stderr
eprint("частичный stderr");
```

`println` принимает любое количество аргументов любых типов благодаря variadic templates C++.

---

## Модуль visibility: pub

```flux
pub fnc exported() -> int32 { ... }   // видна при #import
fnc private_func() -> () { ... }       // только в текущем файле

pub struct Point { ... }    // структура видна снаружи
```

---

- [[Стандартная библиотека|Стандартная библиотека — io, math, string]]
- [[Лексер|Все токены и ключевые слова]]
- [[Семантический анализатор|Правила проверки типов]]
