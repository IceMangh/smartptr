#!/usr/bin/env python3
"""Create Seaborn figures and Russian HTML/Markdown reports from recorded data."""
import html
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "reports"
os.environ.setdefault("MPLCONFIGDIR", str(ROOT / "build/matplotlib"))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

PALETTE = {"UnqPtr": "#087F8C", "std::unique_ptr": "#8CC7CB", "ShrdPtr": "#E17948",
           "std::shared_ptr": "#775AA8", "std::make_shared": "#344B63",
           "Stack<UnqPtr>": "#087F8C", "Stack<std::unique_ptr>": "#E17948"}
ORDER = list(PALETTE)[:5]
SCENARIOS = {"lifecycle": "Полный цикл", "allocate": "Создание", "read": "Чтение",
             "move": "Перемещение", "copy": "Копирование + удаление копий",
             "array": "Массив", "stack": "Стек"}
sns.set_theme(style="whitegrid", context="notebook", font="DejaVu Sans", rc={
    "figure.facecolor": "#FAF9F6", "axes.facecolor": "#FAF9F6", "text.color": "#203444",
    "axes.labelcolor": "#203444", "axes.edgecolor": "#DADDDC", "grid.color": "#E5E6E2",
    "axes.spines.top": False, "axes.spines.right": False, "svg.fonttype": "none"})


def save(fig, name, title, subtitle):
    fig.suptitle(title, fontsize=20, fontweight="bold", x=0.065, ha="left", y=0.985)
    fig.text(0.065, 0.92, subtitle, color="#63717A", fontsize=10)
    fig.text(0.065, 0.025, "SMARTPTR  /  PERFORMANCE LAB     •     Release · один поток · меньше — быстрее", fontsize=9, color="#63717A")
    fig.tight_layout(rect=(0.035, 0.06, 0.99, 0.88))
    for extension in ("png", "svg"):
        path = OUT / "figures" / f"{name}.{extension}"
        fig.savefig(path, dpi=180, facecolor=fig.get_facecolor())
        if extension == "svg":
            path.write_text("\n".join(line.rstrip() for line in path.read_text().splitlines()) + "\n")
    plt.close(fig)


def line_chart(ax, data, order):
    sns.lineplot(data=data, x="n", y="time_ms", hue="implementation", hue_order=order,
                 palette=PALETTE, estimator="median", errorbar=None, marker="o", linewidth=2.4, ax=ax)
    for implementation in order:
        group = data[data.implementation == implementation].groupby("n").time_ms
        low, high = group.quantile(.25), group.quantile(.75)
        ax.fill_between(low.index.to_numpy(), low.to_numpy(), high.to_numpy(), color=PALETTE[implementation], alpha=.16)
    ax.set(xscale="log", yscale="log", xlabel="Число элементов N", ylabel="Время, мс")
    ax.set_xticks([1000, 10000, 100000, 1000000], ["1 тыс.", "10 тыс.", "100 тыс.", "1 млн"])
    ax.legend(title=None, fontsize=9, frameon=False)


def format_number(value):
    return f"{value:,.3f}".replace(",", " ")


def md_table(headers, rows):
    return "\n".join(["| " + " | ".join(headers) + " |", "| " + " | ".join(["---"] * len(headers)) + " |"] +
                     ["| " + " | ".join(map(str, row)) + " |" for row in rows])


def main():
    np.random.seed(20261001)  # Reproducible jitter in the distribution plot.
    (OUT / "figures").mkdir(exist_ok=True)
    df = pd.read_csv(OUT / "raw.csv")
    env = json.loads((OUT / "environment.json").read_text())
    keys = ["scenario", "implementation", "n"]
    if df.duplicated(keys + ["repetition"]).any() or not np.isfinite(df.time_ms).all() or (df.time_ms <= 0).any():
        raise ValueError("Invalid benchmark samples")
    counts = df.groupby(keys).size()
    if len(counts) != 116 or not counts.eq(env["repetitions"]).all():
        raise ValueError("Incomplete benchmark matrix")
    expected = lambda n: (n // 97) * 4753 + (n % 97) * (n % 97 + 1) // 2
    if not df.checksum.eq(df.n.map(expected)).all():
        raise ValueError("Invalid checksum")
    summary = df.groupby(keys, sort=False).agg(
        median_ms=("time_ms", "median"), q25_ms=("time_ms", lambda s: s.quantile(.25)),
        q75_ms=("time_ms", lambda s: s.quantile(.75)), min_ms=("time_ms", "min"),
        max_ms=("time_ms", "max"), median_ns=("ns_per_operation", "median"), samples=("time_ms", "size")).reset_index()
    summary.to_csv(OUT / "summary.csv", index=False, float_format="%.9f")
    focus = df[(df.n == 100000) & (df.scenario == "lifecycle")]
    lifecycle = summary[summary.scenario == "lifecycle"].set_index(["implementation", "n"])
    value = lambda name, n=100000: lifecycle.loc[(name, n), "median_ms"]
    unq_ratio = value("UnqPtr") / value("std::unique_ptr")
    shrd_ratio = value("ShrdPtr") / value("std::shared_ptr")
    make_ratio = value("std::make_shared") / value("std::shared_ptr")

    fig, axes = plt.subplots(1, 2, figsize=(13.5, 6))
    line_chart(axes[0], df[(df.scenario == "lifecycle") & df.implementation.isin(ORDER[:2])], ORDER[:2])
    line_chart(axes[1], df[(df.scenario == "lifecycle") & df.implementation.isin(ORDER[2:])], ORDER[2:])
    axes[0].set_title("Единоличное владение", loc="left", pad=14, fontweight="bold")
    axes[1].set_title("Совместное владение", loc="left", pad=14, fontweight="bold")
    save(fig, "01_scaling", "Как растёт стоимость владения", "Создание вектора → new int → чтение → удаление. Медиана; полоса — 25–75-й процентили. Обе оси логарифмические.")

    fig, ax = plt.subplots(figsize=(12, 6.5))
    sns.boxplot(data=focus, x="time_ms", y="implementation", order=ORDER, hue="implementation", palette=PALETTE,
                legend=False, width=.5, showfliers=False, ax=ax)
    sns.stripplot(data=focus, x="time_ms", y="implementation", order=ORDER, color="#203444", size=3.5, alpha=.55, jitter=.11, ax=ax)
    ax.set(xlabel="Полный цикл, мс", ylabel="")
    ax.grid(axis="y", visible=False)
    for idx, name in enumerate(ORDER):
        ax.text(.985, idx, f"{value(name):.3f} мс", ha="right", va="center", transform=ax.get_yaxis_transform(),
                fontsize=10, fontweight="bold", bbox={"facecolor": "#FAF9F6", "edgecolor": "none", "pad": 3})
    save(fig, "02_distribution", "Скорость и стабильность", f"N = 100 000 · {env['repetitions']} повторение. Каждая точка — отдельный замер; коробка — IQR, линия — медиана.")

    relative_rows = []
    for scenario in SCENARIOS:
        for custom, standard, pair in [("UnqPtr", "std::unique_ptr", "UnqPtr / unique_ptr"),
                                       ("ShrdPtr", "std::shared_ptr", "ShrdPtr / shared_ptr")]:
            group = summary[(summary.n == 100000) & (summary.scenario == scenario)].set_index("implementation")
            if custom in group.index and standard in group.index:
                relative_rows.append({"scenario": SCENARIOS[scenario], "pair": pair,
                                      "ratio": group.loc[custom, "median_ms"] / group.loc[standard, "median_ms"]})
    ratios = pd.DataFrame(relative_rows)
    matrix = ratios.pivot(index="scenario", columns="pair", values="ratio").reindex([SCENARIOS[s] for s in list(SCENARIOS)[:-1]])
    fig, ax = plt.subplots(figsize=(10.5, 6.5))
    sns.heatmap(matrix, annot=True, fmt=".2f", cmap=sns.diverging_palette(170, 25, as_cmap=True), center=1,
                vmin=0, vmax=max(2, float(matrix.max().max())), linewidths=5, linecolor="#FAF9F6",
                cbar_kws={"label": "Время своего / время стандартного"}, ax=ax)
    ax.set(xlabel="", ylabel="")
    ax.grid(False)
    ax.tick_params(axis="y", rotation=0)
    save(fig, "03_ratios", "Где возникает разница", "N = 100 000 · отношение медиан. < 1 — свой быстрее; > 1 — стандартный быстрее. Пусто — операция недоступна.")

    fig, axes = plt.subplots(1, 2, figsize=(13.5, 6))
    line_chart(axes[0], df[df.scenario == "array"], ORDER[:4])
    line_chart(axes[1], df[df.scenario == "stack"], list(PALETTE)[5:])
    axes[0].set_title("Массив: 32 полных цикла", loc="left", pad=14, fontweight="bold")
    axes[1].set_title("Стек: N push + N top/pop", loc="left", pad=14, fontweight="bold")
    save(fig, "04_arrays_stack", "От указателя к структуре данных", "Медиана и IQR · стек сравнивается с таким же связным алгоритмом на std::unique_ptr.")

    rows = []
    for name in ORDER:
        row = lifecycle.loc[(name, 100000)]
        rows.append([name, format_number(row.median_ms), f"{row.q25_ms:.3f}–{row.q75_ms:.3f}",
                     format_number(row.median_ns), int(row.samples)])
    headers = ["Реализация", "Медиана, мс", "IQR, мс", "нс / элемент", "Замеров"]
    table = pd.DataFrame(rows, columns=headers).to_html(index=False, border=0, classes="results")
    operation_rows = []
    for scenario in list(SCENARIOS)[1:]:
        group = summary[(summary.n == 100000) & (summary.scenario == scenario)].set_index("implementation")
        for name in list(PALETTE):
            if name in group.index:
                row = group.loc[name]
                operation_rows.append([SCENARIOS[scenario], name, format_number(row.median_ms), format_number(row.median_ns)])
    op_headers = ["Сценарий", "Реализация", "Медиана, мс", "нс / единицу работы"]
    operation_table = pd.DataFrame(operation_rows, columns=op_headers).to_html(index=False, border=0, classes="results")
    conclusions = [
        f"При N = 100 000 полный цикл UnqPtr занимает {value('UnqPtr'):.3f} мс, std::unique_ptr — {value('std::unique_ptr'):.3f} мс. Отношение времён — {unq_ratio:.2f}×.",
        f"ShrdPtr: {value('ShrdPtr'):.3f} мс; std::shared_ptr(new int): {value('std::shared_ptr'):.3f} мс. Отношение — {shrd_ratio:.2f}×. Сравнение относится к одному потоку; ShrdPtr не синхронизирует счётчик ссылок.",
        f"std::make_shared: {value('std::make_shared'):.3f} мс; отношение к std::shared_ptr(new int) — {make_ratio:.2f}×. Это отдельный способ выделения памяти, показанный как дополнительный ориентир.",
        "Эти замеры показывают поведение конкретной машины и сборки. Небольшие различия при пересекающемся IQR не доказывают устойчивое преимущество; статистическая значимость здесь не проверяется.",
    ]
    methods = [
        "Release, C++20, без санитайзеров и LTO. Тесты корректности выполняются отдельно с AddressSanitizer и UndefinedBehaviorSanitizer; проверки assert включены.",
        f"Четыре размера N: 1 000, 10 000, 100 000, 1 000 000. Для каждого сочетания — 3 прогрева и {env['repetitions']} учитываемое повторение. Всего {len(df):,} измерений. Порядок случаев перемешивается на каждом проходе, seed = 20261001.",
        "steady_clock измеряет стеновое время. Используются медиана и межквартильный интервал (25–75%); IQR описывает разброс замеров и не является доверительным интервалом.",
        "Полный цикл включает создание вектора, N отдельных объектов int, чтение и уничтожение. Создание отдельно: вектор подготовлен заранее, чтение и удаление за границей таймера.",
        "Чтение: 32 прохода по заранее созданным объектам. Перемещение: 16 проходов туда и обратно между двумя подготовленными векторами, 32N move-присваиваний; получатель пустой.",
        "Копирование: 16 созданий вектора копий и его уничтожений. Включены выделение буфера вектора, увеличение и уменьшение счётчиков; нс / единицу — одна пара копирование + уничтожение, а не одна инструкция.",
        "Массив: 32 цикла new int[N], заполнение, чтение, delete[]. Нормирование — на 32N обработанных элементов. Для полного цикла, создания и стека единица работы — один элемент; для чтения — одно разыменование; для перемещения — одно move-присваивание.",
        "Стек: N push и N top/pop. Стандартный контроль реализован тем же связным алгоритмом с итеративным удалением, на std::unique_ptr; это не std::stack с иным контейнером.",
        "Непрозрачная функция в отдельной единице трансляции делает буферы наблюдаемыми для оптимизатора. Все случаи проверяют сумму i % 97 + 1; перемещение дополнительно проверяет пустые источники, копирование — use_count() == 1 после удаления копий.",
    ]
    limitations = [
        "ShrdPtr использует обычный счётчик ссылок. Стандартный shared_ptr поддерживает безопасное изменение владения разными экземплярами в разных потоках; общий набор гарантий у реализаций различается.",
        "Использован системный аллокатор. Создание отдельных int и служебных блоков может доминировать над стоимостью оболочки указателя. Кэш и аллокатор находятся в прогретом состоянии; холодный старт не измерен.",
        "В этой серии не контролируются частота CPU, фоновые процессы и привязка к ядру. Отдельные процессы и повторения на других машинах не выполнялись.",
        "Память, многопоточность, weak_ptr, пользовательские deleter и большие объекты не измерялись. Отчёт не утверждает функциональную эквивалентность своих указателей стандартным.",
    ]
    environment_rows = [["Дата (UTC)", env["timestamp"]], ["CPU", env["cpu"]], ["Система", env["platform"]],
                        ["Архитектура", env["architecture"]], ["Компилятор", env["compiler"].splitlines()[0]],
                        ["Флаги", env["cxx_flags"] + " " + env["release_flags"]], ["Python", env["python"]],
                        ["Seaborn", env["libraries"]["seaborn"]], ["Потоков", "1"], ["Проверка", "CTest: 1/1, ASan + UBSan"]]
    environment_table = pd.DataFrame(environment_rows, columns=["Параметр", "Значение"]).to_html(index=False, border=0, classes="results")
    bullet_html = lambda items: "<ul>" + "".join(f"<li>{html.escape(item)}</li>" for item in items) + "</ul>"
    figure = lambda name, caption: f'<figure><img src="figures/{name}.svg" alt="{html.escape(caption)}"><figcaption>{html.escape(caption)}</figcaption></figure>'
    css = """
    :root{--ink:#203444;--muted:#63717a;--teal:#087f8c;--paper:#faf9f6}*{box-sizing:border-box}
    body{margin:0;background:var(--paper);color:var(--ink);font:16px/1.7 system-ui,-apple-system,sans-serif}
    main{max-width:1120px;margin:auto;padding:70px 36px}header{border-top:5px solid var(--teal);padding-top:25px}
    .eyebrow{text-transform:uppercase;letter-spacing:.18em;font-size:12px;font-weight:700;color:var(--teal)}
    h1{font-size:clamp(38px,6vw,68px);line-height:1.07;letter-spacing:-.045em;max-width:850px;margin:24px 0}
    .lead{font-size:21px;color:var(--muted);max-width:800px}.tags{display:flex;gap:9px;flex-wrap:wrap;margin:24px 0}
    .tags span{background:#eaf1ee;padding:5px 12px;border-radius:30px;font-size:12px}.cards{display:grid;grid-template-columns:repeat(3,1fr);gap:18px;margin:35px 0}
    .card{padding:23px;border:1px solid #dadfdc;border-radius:14px;background:white}.card small{display:block;color:var(--muted)}
    .number{font-size:38px;font-weight:750;letter-spacing:-.035em;color:var(--teal)}.card p{font-size:13px;margin:8px 0 0}
    section{margin-top:55px}h2{font-size:29px;letter-spacing:-.025em;margin-bottom:12px}h3{font-size:19px}p{margin:12px 0}
    figure{margin:25px -12px}figure img{width:100%;height:auto}figcaption{font-size:12px;color:var(--muted);padding:0 12px}
    table{border-collapse:collapse;width:100%;font-size:14px}th{text-align:left;background:#eaf1ee;font-weight:650}
    td,th{padding:12px 15px;border-bottom:1px solid #dce1de}tbody tr:hover{background:#edf4f1}
    .table-wrap{overflow:auto}li{margin:11px 0}ul{padding-left:22px}.note{border-left:4px solid #e17948;background:#f7eee5;padding:18px 24px;border-radius:0 10px 10px 0}
    a{color:var(--teal)}pre{padding:22px;background:#203444;color:#eff5f3;border-radius:12px;overflow:auto;font-size:13px}
    footer{margin-top:60px;padding-top:24px;border-top:1px solid #dce1de;font-size:12px;color:var(--muted)}
    @media(max-width:680px){main{padding:30px 18px}.cards{grid-template-columns:1fr}td,th{padding:9px}figure{margin:20px 0}}
    @media print{main{padding:0}.card,figure,tr{break-inside:avoid}h2{break-after:avoid}body{font-size:11pt}.number{font-size:25pt}section{margin-top:24px}}
    """
    reproduction = "python3 -m venv .venv\n.venv/bin/python -m pip install -r benchmarks/requirements.txt\n.venv/bin/python benchmarks/run.py\n\n# Перестроить оформление из сохранённых данных:\n.venv/bin/python benchmarks/report.py"
    document = f"""<!doctype html>
<html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmartPtr — сравнение производительности</title><style>{css}</style></head><body><main>
<header><div class="eyebrow">Smartptr / Performance study / {env['timestamp'][:10]}</div>
<h1>Цена владения.<br>В цифрах и графиках.</h1>
<p class="lead">Сравнение UnqPtr и ShrdPtr со стандартными умными указателями C++: от создания объекта до работы связного стека.</p>
<div class="tags"><span>C++20 · Release</span><span>Seaborn {env['libraries']['seaborn']}</span><span>21 повторение</span><span>4 масштаба</span><span>ASan + UBSan: пройдено</span></div></header>
<div class="cards"><div class="card"><small>UnqPtr / std::unique_ptr</small><div class="number">{unq_ratio:.2f}×</div><p>Отношение медиан полного цикла · N = 100 000</p></div>
<div class="card"><small>ShrdPtr / std::shared_ptr</small><div class="number">{shrd_ratio:.2f}×</div><p>Однопоточное сравнение · N = 100 000</p></div>
<div class="card"><small>Проверенных замеров</small><div class="number">{len(df):,}</div><p>Каждый результат подтверждён контрольной суммой</p></div></div>
<div class="note">Значение меньше 1× означает меньшее время у своего указателя. Отношение времён не является оценкой статистической значимости.</div>
<section><div class="eyebrow">01 / Главный результат</div><h2>Полный жизненный цикл</h2><p>Создание, чтение и уничтожение объектов; стоимость выделения вектора тоже включена.</p>
{figure('01_scaling', 'Масштабирование полного цикла: медиана и межквартильный интервал.')}
<div class="table-wrap">{table}</div>{figure('02_distribution', 'Все повторения при N = 100 000. Медианы подписаны справа.')}</section>
<section><div class="eyebrow">02 / Разбор операций</div><h2>Разница зависит от сценария</h2>
{figure('03_ratios', 'Отношения медиан своих и стандартных реализаций.')}
<details><summary>Подробная таблица операций при N = 100 000</summary><div class="table-wrap">{operation_table}</div></details></section>
<section><div class="eyebrow">03 / Практические нагрузки</div><h2>Массивы и связный стек</h2>
{figure('04_arrays_stack', 'Массивы и стеки: одинаковая работа, разные оболочки владения.')}</section>
<section><div class="eyebrow">04 / Интерпретация</div><h2>Что следует из измерений</h2>{bullet_html(conclusions)}</section>
<section><div class="eyebrow">05 / Методика</div><h2>Как получены результаты</h2>{bullet_html(methods)}
<h3>Ограничения сравнения</h3>{bullet_html(limitations)}</section>
<section><div class="eyebrow">06 / Воспроизводимость</div><h2>Окружение и повторный запуск</h2><div class="table-wrap">{environment_table}</div>
<pre><code>{html.escape(reproduction)}</code></pre><p><a href="raw.csv">Исходные замеры CSV</a> · <a href="summary.csv">Агрегаты CSV</a> · <a href="environment.json">Полное окружение JSON</a> · <a href="REPORT.md">Отчёт Markdown</a></p></section>
<footer>SmartPtr Performance Lab · Графики построены Seaborn по реальным локальным измерениям. SVG — для печати, PNG — для презентаций.</footer>
</main></body></html>"""
    document = document.replace("<span>21 повторение</span>", f"<span>{env['repetitions']} повторений</span>")
    (OUT / "report.html").write_text(document, encoding="utf-8")
    markdown = f"""# SmartPtr: сравнение производительности

Реальные локальные измерения UnqPtr, ShrdPtr и стандартных указателей C++.
Оформленный HTML-отчёт: [report.html](report.html). Дата UTC: {env['timestamp']}.

## Основные результаты

""" + "\n\n".join("- " + item for item in conclusions) + "\n\n" + md_table(headers, rows) + "\n\n"
    for name, title in [("01_scaling", "Масштабирование"), ("02_distribution", "Разброс замеров"),
                        ("03_ratios", "Отдельные операции"), ("04_arrays_stack", "Массивы и стек")]:
        markdown += f"## {title}\n\n![{title}](figures/{name}.png)\n\n"
    markdown += "## Все операции при N = 100 000\n\n" + md_table(op_headers, operation_rows) + "\n\n"
    for title, items in [("Методика", methods), ("Ограничения", limitations)]:
        markdown += f"## {title}\n\n" + "\n\n".join("- " + item for item in items) + "\n\n"
    markdown += "## Окружение\n\n" + md_table(["Параметр", "Значение"], environment_rows) + "\n\n"
    markdown += "## Воспроизведение\n\n```sh\n" + reproduction + "\n```\n\n"
    markdown += "[Исходные CSV](raw.csv) · [Сводные CSV](summary.csv) · [Окружение JSON](environment.json).\n"
    (OUT / "REPORT.md").write_text(markdown, encoding="utf-8")
    print(f"Generated report.html, REPORT.md and 4 Seaborn figures from {len(df)} samples")


if __name__ == "__main__":
    main()
