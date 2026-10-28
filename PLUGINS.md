# Альтернативный репозиторий плагинов

Адрес, указанный в файле репозитория:

```
https://bit.ly/mprepo
```

Тот же файл: [plugins/repo.json](plugins/repo.json).

Для использования данного альтернативного репозитория плагинов прописать в настройках Movian адрес выше.

Путь: **Settings → General → Alternative plugin repository URL**  
(Настройки → Общие → Alternative plugin repository URL).

Вернуться на домашний экран. Плагины появятся в **Plugins → Browse available plugins**.

P.S.  
Если вдруг в альтернативном репозитории категории остаются пустыми или плагины в них исчезли, либо при установке плагинов выдает ошибку, то пробуем использовать сервисы VPN.

Чтобы вернуться к официальному репозиторию, прописанный в настройках Movian адрес альтернативного репозитория полностью удаляем, оставив поле пустым.

На PS5 тот же экран открывает **Options → Repository** в `payload.elf`: **Settings → General → Alternate plugin Repository URL**. Пустое поле — официальный репозиторий. Влево/вправо переключает готовые адреса, включая `https://bit.ly/mprepo` и прямой файл на GitHub (консоль не ходит по редиректам bit.ly).

## English

Enter this address in Movian:

```
https://bit.ly/mprepo
```

**Settings → General → Alternative plugin repository URL**

Go back to the home screen. Plugins are listed under **Plugins → Browse available plugins**.

If categories stay empty, plugins disappear, or install fails, try a VPN.

To use the official repository again, delete the address and leave the field empty.

## On the PS5 payload

The **Options → Repository** row opens the same General page inside `payload.elf`. The Options button itself is the player menu, and on a plugin page it lists that page's settings.

- Left / Right on **Alternate plugin Repository URL** cycles four values: empty (official `https://repo.movian.eu/plugins-v1.json`), `https://bit.ly/mprepo`, the raw file on GitHub, and the official URL written out.
- Cross opens the on-screen keyboard. Triangle shifts letters. Square or L1 erases. R1 or **done** saves. Circle cancels.
- **Browse available plugins** downloads the feed and installs the package you pick into a directory L2 already scans (`/data/movian/plugins`, `/mnt/usb0/plugins`, `/mnt/usb1/plugins`, `/user/movian/plugins`).
- **Automatically upgrade plugins**, when on, upgrades an installed package after that fetch if the feed's version is newer. **Upgrade installed plugins** does that even when the switch is off.
- **Auto install plugins on demand** installs a missing package when a page URL matches a `control.uriprefixes` entry from a feed that has already been fetched. This repository's `plugins/repo.json` does not publish those prefixes.
- Beta passwords are sent as repeated `betapassword` query arguments, the same as M7.
- Hold Options still quits. Rescan USB is the last settings row.
- The console HTTP client does not follow redirects. `https://bit.ly/mprepo` needs one. If that preset fails to load, use the raw GitHub URL in the same Left/Right cycle.
