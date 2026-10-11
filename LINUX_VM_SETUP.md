# Linux-VM для визуальных тестов плагинов

Цель: виртуалка на этом маке с Linux-десктопом и REAPER, в которой Claude по SSH сам запускает
плагины, снимает скриншоты экрана и кликает (`xdotool`). Ты в это время можешь смотреть в окно VM.

Это дополнение к Docker-харнессу (`linux-research/`), а не замена: VM даёт настоящую сессию с
оконным менеджером, реальное встраивание окна в хост и фокус клавиатуры.

---

## 1. Что ставить

| | Выбор | Почему |
|---|---|---|
| Гипервизор | **UTM** (бесплатно, https://mac.getutm.app или App Store) | нативная ARM-виртуализация, ускорение OpenGL через virgl. Parallels тоже подойдёт, если он уже есть |
| Дистрибутив | **Ubuntu 24.04 LTS Desktop, arm64** (последний point-release, `ubuntu-24.04.x-desktop-arm64.iso` с https://ubuntu.com/download/desktop или https://cdimage.ubuntu.com/releases/24.04/release/) | у 24.04 ещё есть сессия **«Ubuntu on Xorg»**. В Ubuntu 25.10 и Fedora 41+ GNOME уже только Wayland, а под Wayland `xdotool` и скриншоты извне не работают |
| Если arm64-desktop-ISO не найдётся | Ubuntu 24.04 **Server arm64**, затем `sudo apt install ubuntu-desktop` | итог тот же |

Почему не x86_64: на маке x86-Linux пойдёт только через эмуляцию, а это очень медленно. ARM-VM
нужна для разработки и визуальных проверок. Финальный прогон x86_64-билдов (их будут качать
пользователи) и хостов без arm64-версии (**Bitwig — только x86_64**) нужно делать на живом PC.

## 2. Создание VM в UTM

1. **Create a New Virtual Machine → Virtualize → Linux.**
2. Boot ISO: выбрать скачанный образ. Опцию «Use Apple Virtualization» **не** включать: нужен QEMU-бэкенд,
   у него есть ускорение OpenGL.
3. Ресурсы: **4 ядра, 8 ГБ RAM** (можно 6 ГБ, если тесно), диск **64 ГБ**.
4. Перед первым запуском открыть настройки VM:
   - **Display → Emulated Display Card: `virtio-gpu-gl-pci`** и включить **GPU Acceleration (OpenGL)**.
     Если рабочий стол глючит или чёрный, переключить на `virtio-gpu-pci` без GL: тогда работает программный
     llvmpipe, для NanoVG этого достаточно.
   - **Network → Shared Network** (по умолчанию). VM получит адрес вида `192.168.64.x`, доступный с мака.
   - Разрешение экрана: удобно 1920×1080.
5. Установить Ubuntu как обычно. Имя пользователя любое, его потом пришлёшь мне.
6. После установки извлечь ISO (CD/DVD → Clear) и перезагрузиться.

## 3. Настройка внутри VM

Всё ниже выполняется в терминале VM.

### 3.1 Обновление и базовые пакеты

```bash
sudo apt update && sudo apt full-upgrade -y
```

```bash
sudo apt install -y openssh-server spice-vdagent xdotool wmctrl x11-utils imagemagick scrot gdb mesa-utils git curl
```

Остальное (компиляторы, dev-пакеты, REAPER) поставлю сам по SSH.

### 3.2 Принудительно Xorg и автологин

Сессия на `:0` должна существовать всегда, даже если никто не залогинен руками.

```bash
sudo nano /etc/gdm3/custom.conf
```

В секции `[daemon]` должно быть (раскомментировать или дописать; `USERNAME` заменить на своё имя):

```
[daemon]
WaylandEnable=false
AutomaticLoginEnable=true
AutomaticLogin=USERNAME
```

`WaylandEnable=false` надёжнее, чем выбирать «Ubuntu on Xorg» на экране входа: при автологине
экрана входа нет.

### 3.3 Отключить блокировку, гашение экрана и сон

Выполнять в графической сессии VM (в терминале на её рабочем столе):

```bash
gsettings set org.gnome.desktop.screensaver lock-enabled false
```

```bash
gsettings set org.gnome.desktop.session idle-delay 0
```

```bash
gsettings set org.gnome.settings-daemon.plugins.power sleep-inactive-ac-type 'nothing'
```

Ещё стоит выключить уведомления: Settings → Notifications → Do Not Disturb. Иначе они влезают в скриншоты.

### 3.4 SSH-ключ для Claude

```bash
mkdir -p ~/.ssh && chmod 700 ~/.ssh
```

```bash
echo 'ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIPjs0hvihFg8bX7daYPHclIZRJ9j0KiupMTT6NQU3nAd claude-linux-vm' >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys
```

Приватная половина ключа лежит на маке в `~/.ssh/claude_linux_vm` и используется только для этой VM.

Чтобы я мог ставить пакеты без пароля, нужен беспарольный `sudo` для твоего пользователя. Это только
внутри VM, на мак не влияет:

```bash
echo "$USER ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/90-claude
```

### 3.5 Перезагрузка и проверка

```bash
sudo reboot
```

После перезагрузки VM сама должна попасть на рабочий стол. Проверка, что сессия X11:

```bash
echo $XDG_SESSION_TYPE
```

Должно быть `x11`.

### 3.6 Адрес VM

```bash
ip -4 addr show | grep inet
```

Пришли мне адрес вида `192.168.64.x` и имя пользователя. Дальше я сам:
- проверю доступ (`ssh -i ~/.ssh/claude_linux_vm user@192.168.64.x`);
- поставлю сборочные пакеты (тот же набор, что в `linux-research/docker/Dockerfile`);
- скачаю и поставлю REAPER aarch64;
- настрою скриншоты и `xdotool` на `DISPLAY=:0`;
- буду раскладывать сборки в `~/.clap` и `~/.vst3` через `rsync`, или собирать прямо в VM.

## 4. После настройки

- **Сделай клон VM** (UTM → правый клик → Clone) как чистую базу. Если тесты что-то сломают,
  вернёмся к ней.
- Во время тестов VM должна быть запущена. Окно можно свернуть, сессия при этом живёт.
- Хосты, которые можно проверить в arm64-VM:
  - **REAPER** — есть aarch64.
  - **Ardour** — есть сборки для arm64 Linux (проверить на ardour.org); грузит только VST3.
  - **Carla** — через репозиторий KXStudio или сборку из исходников.
  - **Renoise** — есть arm64 Linux-версия (проверить).
  - **Bitwig** — нет, только x86_64.
- Wayland проверим позже, отдельно: вернуть `WaylandEnable=true` и войти в обычную сессию. Тогда
  плагины пойдут через XWayland, а автоматизация через `xdotool` работать не будет — там смотришь глазами ты.

## 5. Ограничения VM

- GPU виртуальный (virgl или llvmpipe). Баги конкретных драйверов NVIDIA/AMD так не поймать.
- Архитектура arm64. Поведение x86_64-билдов и сборку со старым glibc проверяем отдельно: Docker
  (`--platform linux/amd64`) и живой PC.
- Я вижу экран кадрами, видео нет. Плавность анимаций оцениваю по серии снимков и логам FPS.
