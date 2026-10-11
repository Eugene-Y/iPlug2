# 07 — Слияние upstream iPlug2 master в `hvoya`: анализ

Дата: 2026-10-11. `hvoya` = `46f834a92`, `upstrem/master` = `d54f69050` (2026-08-19), merge-base = `f98027a70` (2026-03-22).
Пробная ветка: `merge-trial/upstream-master` (коммит `60e460eff`), worktree `linux-research/wt/merge-trial`. `hvoya` не тронута, ничего не запушено, в репозиторий Gneiss ничего не записано.

## TL;DR

- В `hvoya..upstrem/master` 242 коммита: 214 обычных и 28 merge-коммитов. **В IPlug/CLAP, VST3, AU, AAX, IGraphicsMac/Win, NanoVG/Metal, Skia, параметры и state нет ни одного изменения.** Visage, CLI и прочих крупных новинок в master тоже нет. ~73% (157 коммитов) — это поставка WDL от Cockos, остальное — ReaperExt, Wasm/Web и CMake.
- Реальный конфликт один: `IPlug/APP/IPlugAPP_main.cpp`, тривиальный. Upstream (`c6817ba3b`) по-другому чинит то же, что в hvoya закомментировано в `e26ea1ba9` «TODO FIXME». Взята версия upstream.
- Gneiss macOS Debug (APP, CLAP, VST3) против слитого дерева: **BUILD SUCCEEDED**, новых предупреждений 0, одно пропало. Базовая сборка на `hvoya` такая же зелёная.
- Рекомендация: **сливать полностью, но не перед релизом.** Риск низкий, практической пользы для Gneiss мало. Главный аргумент за — меньше разрыв с upstream перед Linux-портом: ветка `linux-trial/rangerrick` пересекается с upstream в 16 файлах, в 8 из них будут конфликты.

## 1. Инвентарь upstream-коммитов

Подсчёт по 214 обычным коммитам, группировка по затронутым путям и теме.

| Группа | Коммитов | Что приносит | Для Gneiss |
|---|---|---|---|
| WDL/swell: macOS (cocoa) и общий код | 45 | Режимы масштабирования диалогов (`38d3838fa`, `c3ed61afc`, `6a326cf4c`), новый синтаксис ресурсов `SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2` и генератор `swell_resgen` (`1d265ef78`, `c7238bc84`), метрики шрифтов CoreText (`f740b52e4`, `edeeeaac1`, `d4b5750d7`, `5fbf4d9c3`, `091016a22`, `0676ce949`, `17c54d9e3`), `clipsToBounds=YES` (`73a958328`), overpaint при отрицательном rect (`ec3306019`), сборка на свежем clang и Xcode 26 CLT (`4032d3316`, `f1bc6c197`), `SWELL_GetOSXVersion` для macOS 26 (`e12c0a735`), Metal dirty-list (`c30e5a918`, `e7034b6d4`), валидатор DEFPUSHBUTTON (`985316222`) | **Касается только APP на macOS**: окно, меню, диалог Preferences. Полезно, но внешний вид диалога может измениться. Подробнее в §4 |
| WDL/swell: Linux (generic, gdk, freetype, headless) | 36 | XDND-прокси для bridge-окон (`8166e077e`, `15345c42d`, `7e44f2017` и др.), пересылка клавиатуры из bridge-окна (`e5b32c175`, `d1bc62b5b`, `2f6f4ee40`), метрики freetype (`7f53d0d3e`, `f4df2d1ee`, `51be0fedf`), запрет скринсейвера | Для Gneiss на mac/win не важно. **Для Linux-порта полезно** |
| WDL: Windows и UI-мелочи (win32_utf8, curses, virtwnd, coolscroll, localize, langpack) | 34 | Хуки `WDL_UTF8_Hook*` с assert-проверкой имени класса (`8b3c47ecb`), `LVM_SETUNICODEFORMAT` (`4f90953b0`), GlobalAlloc в listview-хуке (`d85b616e3`), `GetFileAttributesUTF8` и `AddFontResourceExUTF8` | Из этого в APP на Windows используется `win32_utf8.c`: комбобоксы аудио- и MIDI-устройств в Preferences. Остальное не компилируется |
| WDL: прочее (lice, eel2, metadata, jnetlib, has_strings, wdltypes, resample, time_series) | 41 | Шаблоны `wdl_freefunc` и `wdl_cmpfunc`, глифы lice, метаданные, keep-alive в jnetlib, `WDL_Resampler::GetCurrentLatency` (`4f04d5c96`) | Не используется. `WDL_Resampler` нет ни в Gneiss, ни в hvoya utils. `wdltypes.h` компилируется, но на mac проблем нет |
| Wasm / Web / WAM | 33 | Разделение Wasm DSP/UI в CMake, host-controls, dev-сервер, **нативные popup-меню в `IGraphicsWeb`** (`1de13d47c`), устранение мерцания при resize (`6c374f40a`), отложенный resize в WasmUI (`4cf5c46aa`), сохранение id канваса (`d243083cd`), переход на `__EMSCRIPTEN__` (`7bbae8448`, `fccdffd14`) | Gneiss собирает web по старому WAM-пути (`Gneiss-web.mk`, `IPlug/WEB/Template`). `1de13d47c` правит `common-web.mk` (`_iplug_popup_menu_selected`) и делает рабочим `CreatePopupMenu` в вебе: сейчас там меню Gneiss не открываются. Полезно, **если web-версия жива** (см. оговорку в §5) |
| ReaperExt | 15 | Подменю Extensions, состояние на проект, post-command hook, пример с WebUI, исправления toggle и DPI | Не нужно |
| CMake (не Wasm) | 5 | `CMAKE_CXX_STANDARD` можно переопределить (`2416e481e`, `2efee2716`), `AAX_SDK_DIR` (`5a97bdeb3`), PkgInfo в путях с пробелами (`d196dd73e`), тише warnings под emcmake (`f317e8994`) | Gneiss собирается через Xcode/VS, поэтому не нужно. Для Linux-порта (CMake) важно |
| IGraphics core | 2 (в составе Web) | `bd45109b2`: live-edit можно включить в Release через `IPLUG_LIVE_EDIT` и получать JSON-события. `6c374f40a`: добавлен виртуальный `IGraphics::PostResize()` (no-op везде, кроме Web) | Нейтрально. На mac/win поведение не меняется |
| APP | 1 | `c6817ba3b`: guard `#ifdef ID_SCREENSHOT` | **Дублирует hvoya `e26ea1ba9`**, см. §2 |
| Прочее | 4 | nlohmann/json (`4a4a839db`, используется только WebView), убран `#pragma once` из .cpp (`bed4cb93b`), `CUSTOM_BUSTYPE_FUNC` в примере (`26e480ecf`), диагностика WebView в песочнице (`aebb1370c`) | Не нужно (Gneiss не использует WebView и json) |
| Примеры, документация, CI | ~14 | IPlugWebUI, P5js, Svelte, CI-джоб WASM | Не нужно |

### Багфиксы на путях, которые использует Gneiss

- **NanoVG/Metal, текст, popup-меню, файловые диалоги, resize и scale в плагине:** на macOS и Windows таких фиксов нет. Аналоги есть только для Web: `1de13d47c` (popup), `6c374f40a` и `4cf5c46aa` (resize).
- **CLAP, VST3, AU, AAX: state, параметры, латентность, MIDI:** изменений нет. Собственные фиксы hvoya (`5ceb702de`, `eee9468ee`, `0433e9e39`, `3992bff17`, `c8234d338`, `949176b1c` и др.) остаются как есть, конкурентов у них нет.
- **APP на macOS (через SWELL):** `73a958328` (clipsToBounds), `ec3306019` (overpaint), серия про шрифты и scaling диалогов, `d8f17d728` (закрытие Color/Font chooser с клавиатуры, Gneiss их не использует), `4032d3316`, `f1bc6c197` и `e12c0a735` (совместимость с Xcode 26 и macOS 26).
- **APP на Windows:** `win32_utf8` (комбобоксы устройств).

### Дубли фиксов hvoya (семантические горячие точки)

| Место | hvoya | upstream | Итог |
|---|---|---|---|
| `IPlugAPP_main.cpp`: шорткат скриншота при отсутствии `ID_SCREENSHOT` в `resource.h` у Gneiss | `e26ea1ba9`: строка закомментирована | `c6817ba3b`: `#ifdef ID_SCREENSHOT` | Вариант upstream строго лучше. Взят он |
| Мгновенная перерисовка после resize | `98e450ba2`: публичный `virtual DrawNow()`, реализован в IGraphicsMac, вызывается из кода плагина | `6c374f40a`: приватный `virtual PostResize()` в конце `IGraphics::Resize`, реализован только в IGraphicsWeb | Конфликта нет: разные платформы, обе функции no-op по умолчанию. Задача одна и та же («не показывать старый кадр в новых границах»). В будущем `IGraphicsMac::PostResize()` может вызывать `DrawNow()`, и тогда ручные вызовы из плагина станут не нужны. Пока трогать не надо |
| Глюки окна APP при resize на macOS | `cf2b2a1f4`: `EditorResize` обходит `SetWindowPos` и вызывает `[NSWindow setFrame:display:NO]` | `73a958328`, `ec3306019`: клиппинг и перерисовка SWELL-окна | Пути разные: hvoya не идёт через SWELL `SetWindowPos`. Но эффект общий, видимый результат resize в APP надо проверить |

Других дублей нет: `git cherry` показывает, что ни один upstream-коммит не попадал в hvoya повторно.

## 2. Реальные конфликты (пробное слияние)

`git merge upstrem/master` в worktree. Пути, которые меняли обе стороны: `IGraphics/IGraphics.cpp`, `IGraphics/IGraphics.h`, `IGraphics/IGraphicsStructs.h`, `IPlug/APP/IPlugAPP_main.cpp`. Слились автоматически все, кроме одного.

| Файл | Ханк | Тип | Решение |
|---|---|---|---|
| `IPlug/APP/IPlugAPP_main.cpp` ~L441 (`SWELLAppMain`, `WM_CREATE`) | hvoya: `//SetMenuItemModifier(menu, ID_SCREENSHOT, …)`. upstream: то же внутри `#ifdef ID_SCREENSHOT` | тривиальный | Взята версия upstream. В Gneiss `ID_SCREENSHOT` не определён, так что поведение не меняется: шорткат не ставится. Плагины, где он определён, получают шорткат обратно. Это убирает «TODO FIXME» hvoya |

### Пересечения без текстового конфликта (обе стороны трогали одну функцию)

1. **`IGraphics::Resize()`.** Изменения hvoya: `mInOpenWindow` подавляет `EditorResizeFromUI` во время `OpenWindow` (`c8234d338`). Upstream добавил вызов `PostResize()` после `LayoutUI` (`6c374f40a`). Порядок после слияния корректный: PlatformResize → OnResize → DrawResize → LayoutUI → PostResize. На mac и win `PostResize` пустой.
2. **`IGraphics.h`.** hvoya добавила `DrawNow()`, `BringControlToFront()` и `mInOpenWindow`. Upstream добавил `PostResize()`, `SetLiveEditEventFunc()`, `EmitLiveEditEvent()` и `mLiveEditEventFunc`. Независимые добавления, меняется только vtable (нужна полная пересборка, бинарной совместимости тут нет).
3. **Guard'ы live-edit.** Upstream заменил `#ifndef NDEBUG` на `!defined(NDEBUG) || defined(IPLUG_LIVE_EDIT)` в шести местах `IGraphics.cpp`. Новый код hvoya под `NDEBUG` не попал. Два оставшихся `#ifndef NDEBUG` (L947, L967) относятся к `mShowControlBounds` и так и задуманы.
4. **`IGraphicsStructs.h`.** `IText::SetFont` в hvoya и `ILiveEditEventFunc` в upstream — разные места.
5. **SWELL и APP на macOS** (см. дубли выше): `IPlugAPP.cpp` в hvoya не пересекается с правками swell-cocoa в коде, но влияет на тот же визуальный результат.

## 3. Проверка компиляции

**Метод.** В `.xcodeproj` Gneiss пути к IPlug2 заданы относительно (`../../../IPlug2/...`), а `Gneiss-mac.xcconfig` подключает `#include "../../../IPlug2/common-mac.xcconfig"`. Поэтому одного переопределения `IPLUG2_ROOT` мало. Для каждого варианта (merge и base) Gneiss скопирован в scratchpad (`b-<v>/hvoya/Gneiss`, без `build-mac`, `research`, `Backups`, `*.RPP`), рядом лежит симлинк `b-<v>/IPlug2`, указывающий на пробный worktree или на основной checkout. В копии `project.pbxproj` одна абсолютная группа `/Volumes/Storage/code/IPlug2/hvoya/hvoya` перенаправлена в тот же симлинк. По логу компилятора проверено, что `IGraphics.cpp`, `IPlugAPP_main.cpp`, `swell-dlg.mm` и `user_tab_panel.cpp` брались из `b-merge/IPlug2/...`. В worktree скопированы VST3/CLAP SDK из основного checkout (они untracked и ignored).
Команда: `xcodebuild -scheme macOS-{APP,CLAP,VST3} -configuration Debug -arch arm64 -derivedDataPath …/scratchpad/dd-{merge,base} CODE_SIGNING_ALLOWED=NO`. Пути установки `VST3_PATH`, `CLAP_PATH`, `APP_PATH` и др. переопределены в scratchpad, поэтому установленные в `~/Library/Audio/Plug-Ins` плагины не перезаписаны. Xcode 26.5 (17F42).

| Схема | hvoya (база) | merge | Новые предупреждения |
|---|---|---|---|
| macOS-APP | SUCCEEDED, 15 уник. warnings | SUCCEEDED, 14 | 0. Пропало `-Wvla-cxx-extension` в `swell-kb.mm` (upstream сделал `maxStringLength` константой) |
| macOS-CLAP | SUCCEEDED, 12 | SUCCEEDED, 12 | 0 |
| macOS-VST3 | SUCCEEDED, 11 | SUCCEEDED, 11 | 0 |

Логи лежат в `scratchpad/logs/{base,merge}-macOS-*.log`.

**Побочный эффект, который будет в реальном репозитории.** Build phase запускает `swell_resgen.sh ../resources/main.rc`, и новый генератор пишет `resources/main.rc_mac_dlg` в формате `SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2` с `#ifdef SET_IDD_DIALOG_PREF_SCALE`. Этот файл **отслеживается в git Gneiss**: после слияния первая сборка его изменит, и изменение нужно закоммитить в Gneiss. Заодно диалог Preferences переходит на динамическое масштабирование SWELL вместо фиксированного 1.7. `main.rc_mac_menu` не меняется.

**Не проверено:** AUv2, AUv3 и AAX на mac (схема AUv2 запускает `clear_audiounit_caches.command` и трогает системный кеш AU), iOS, вся сборка под Windows (здесь нет MSVC), Web/WAM. Приложение не запускалось, чтобы не трогать `settings.ini` и аудиоустройства. Статический валидатор диалогов (`985316222`) не должен срабатывать: в `main.rc` один `DEFPUSHBUTTON`.

## 4. Рекомендация

**Сливать полностью, когда удобно, но не в ветку, которая уходит в релиз в ближайшие дни.**

Почему полностью, а не cherry-pick:
- Один тривиальный конфликт, сборка чистая, код форматов и рисования плагина не затронут. Регрессий в плагинах ждать неоткуда.
- 157 из 214 коммитов — это WDL. Выдёргивать из него отдельные коммиты неудобно: серия swell-cocoa внутренне связана (scaling → resgen → метрики), а перенос по одному коммиту гарантирует конфликты при следующем слиянии.
- Linux-порт целится в upstream и пересекается с этими изменениями: `git merge-tree merge-trial/upstream-master linux-trial/rangerrick` даёт 8 конфликтующих файлов (`IPlugPlatform.h`, `CMakePresets.json`, `Scripts/cmake/{APP,CLAP,IGraphics,IPlug,ReaperExt,VST3}.cmake`). Если сначала влить upstream в hvoya, эти конфликты придётся решать один раз и на актуальной базе.

Почему не срочно: для плагинов Gneiss на mac и win пользы почти нет. Видимые изменения касаются только APP (диалог Preferences, поведение окна) и Web.

### Что тестировать после слияния (оценка ~2–3 часа)

| Область | Где | Что проверить | Почему |
|---|---|---|---|
| APP на macOS | standalone | Диалог Preferences: шрифты, обрезка подписей, комбобоксы устройств, кнопка OK/Enter. Resize окна с разными размерами UI (регрессия `cf2b2a1f4`/`98e450ba2` + `73a958328`). Меню, горячие клавиши, dead keys | Серия swell-cocoa, `BEGIN2`, clipsToBounds, `swell-kb.mm` |
| APP на Windows | standalone | Сборка всех vcxproj. Preferences: устройства с не-ASCII именами, в Debug не срабатывают assert'ы `WDL_UTF8_Hook*` | `win32_utf8.c`, `wdltypes.h` (MSVC здесь не проверялся) |
| Плагины на mac | REAPER (VST3, CLAP, AU), Logic (AU), Pro Tools (AAX) | Smoke-тест: загрузка, открытие и закрытие UI, resize/scale, popup-меню, сохранение и загрузка пресета через файловый диалог, восстановление проекта (cold-start REAPER), латентность | IGraphics.h/.cpp пересобраны: новые виртуальные функции и guard'ы. Изменений по существу нет, это санитарная проверка |
| Плагины на Windows | REAPER (VST3, CLAP), Pro Tools (AAX) | То же | Только перекомпиляция |
| iOS / AUv3 | — | Сборка | Upstream изменений нет, только перекомпиляция |
| Web/WAM | браузер | Только если web-сборка Gneiss поддерживается: popup-меню, resize | `1de13d47c`, `6c374f40a`. ⚠ `Gneiss-web.mk` компилирует только `Gneiss.cpp` без hvoya utils. Похоже, web-сборка уже не собирается независимо от этого слияния |

### Если всё-таки cherry-pick

Минимальный набор, полезный Gneiss:

1. `c6817ba3b` — guard `ID_SCREENSHOT`. Заменяет `e26ea1ba9` (при cherry-pick будет тот же конфликт с тем же решением).
2. `4032d3316`, `f1bc6c197`, `e12c0a735` — совместимость swell-cocoa со свежим clang, Xcode 26 CLT и macOS 26. Сейчас Gneiss собирается и без них, так что это задел на будущее.
3. `73a958328`, `ec3306019` — клиппинг и overpaint SWELL-окон в APP на macOS. Брать, если у APP остались артефакты при resize.
4. Для Web: `1de13d47c`, `6c374f40a`, `4cf5c46aa`, `d243083cd` (в этом порядке; `1de13d47c` правит и `common-web.mk`).

Остальное (ReaperExt, Wasm/CMake, eel/lice/metadata/jnetlib, swell-generic) Gneiss на mac и win не нужно. swell-generic и CMake нужны только Linux-порту.

## 5. Побочные находки

- `IText::SetFont` в hvoya (`1ac6ffcfc`, `IGraphicsStructs.h`) использует `strncpy(mFont, …, sizeof(mFont))`. Если имя шрифта длиннее или равно `FONT_LEN`, строка останется без завершающего NUL. Upstream этот код не трогал, к слиянию это не относится. Стоит отдельно заменить на `strncpy` + явный `mFont[sizeof(mFont)-1] = '\0'`.
- Комментарий в upstream-версии guard'а (`IPlugAPP_main.cpp`, «mirrors the existing #ifdef pattern…») пересказывает код. Оставлен как есть, чтобы не создавать лишний diff с upstream.

## Артефакты

- Ветка `merge-trial/upstream-master` (`60e460eff`), worktree `linux-research/wt/merge-trial` (оставлены для инспекции; в worktree скопированы VST3/CLAP SDK — ignored).
- Сборочные копии и логи: `/private/tmp/claude-501/-Volumes-Storage-code-IPlug2/30921caf-26d5-4006-a05d-ba2071b2a0b5/scratchpad/{b-merge,b-base,dd-merge,dd-base,logs,install-*}`.
