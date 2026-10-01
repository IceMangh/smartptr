# SmartPtr · Performance Lab

Учебные реализации умных указателей C++20 и связного стека, с воспроизводимым сравнением производительности.

**[Открыть отчёт на GitHub →](reports/REPORT.md)** · [HTML-отчёт](reports/report.html) · [Исходные измерения](reports/raw.csv)

![Сравнение скорости умных указателей](reports/figures/01_scaling.png)

В отчёте: 2 436 реальных измерений, четыре размера нагрузки от 1 000 до 1 000 000 элементов, 21 повторение и три прогрева. Графики Seaborn показывают медиану, межквартильный интервал и отдельные замеры. Также доступны SVG для печати и PNG для презентаций.

## Что сравнивается

| Реализация | Владение | Стандартный ориентир |
| --- | --- | --- |
| `UnqPtr<T>` и `UnqPtr<T[]>` | Единоличное, с перемещением | `std::unique_ptr` |
| `ShrdPtr<T>` и `ShrdPtr<T[]>` | Совместное, со счётчиком ссылок | `std::shared_ptr` |
| `Stack<T>` | Связные узлы на `UnqPtr` | Тот же алгоритм на `std::unique_ptr` |

Измеряются полный жизненный цикл, создание, чтение, перемещение, копирование совместного владения, массивы и стек. `std::make_shared` показан отдельно как другой способ выделения памяти.

`ShrdPtr` использует обычный счётчик ссылок и не предназначен для одновременного изменения владения в разных потоках. Замеры выполнены в одном потоке; полная методика и ограничения приведены в отчёте.

## Повторить измерения

Нужны CMake 4.0+, Clang или GCC с поддержкой C++20 и Python 3.11+. Python-зависимости фиксированы в `benchmarks/requirements.txt`.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r benchmarks/requirements.txt
.venv/bin/python benchmarks/run.py
```

Скрипт собирает отдельный Release-бенчмарк, запускает тесты корректности с AddressSanitizer и UndefinedBehaviorSanitizer, выполняет замеры и пересоздаёт весь отчёт. Санитайзеры в бенчмарке отключены. Новые результаты заменяют файлы в `reports/`.

```sh
# Только пересоздать графики и отчёт по сохранённым CSV:
.venv/bin/python benchmarks/report.py

# Другое число повторений, не меньше трёх:
.venv/bin/python benchmarks/run.py --repetitions 31
```

Для просмотра оформленного HTML скачайте или клонируйте репозиторий и откройте `reports/report.html` в браузере. HTML использует изображения из соседней папки `figures/`; подключение к интернету не требуется. На GitHub удобнее читать `reports/REPORT.md`.

## Файлы отчёта

| Файл | Назначение |
| --- | --- |
| [reports/report.html](reports/report.html) | Оформленный отчёт с таблицами, выводами и стилями для печати |
| [reports/REPORT.md](reports/REPORT.md) | Версия для просмотра на GitHub |
| [reports/figures](reports/figures) | Четыре графика в форматах PNG и SVG |
| [reports/raw.csv](reports/raw.csv) | Каждый замер, его длительность, число операций и контрольная сумма |
| [reports/summary.csv](reports/summary.csv) | Медианы, квартильные границы, минимум и максимум |
| [reports/environment.json](reports/environment.json) | Компилятор, флаги, система, версии библиотек и результат тестов |
| [benchmarks/benchmark.cpp](benchmarks/benchmark.cpp) | Нагрузки и проверки результатов |
| [benchmarks/opaque.cpp](benchmarks/opaque.cpp) | Барьер оптимизации в отдельной единице трансляции |

## Запустить тесты отдельно

```sh
cmake -S . -B build/benchmark-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/benchmark-release --target pointer_tests
ctest --test-dir build/benchmark-release --output-on-failure
```

Исходные `tests.cpp` содержат проверки перемещения, копирования, массивов, сброса владения, самоприсваивания и удаления длинной цепочки стека. Встроенные в них разовые замеры используются как часть тестового вывода; отчёт строится по отдельному `pointer_benchmarks`.
