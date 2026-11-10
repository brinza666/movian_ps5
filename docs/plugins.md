# Плагины

![Plugins](images/plugins.jpg)

Адрес репозитория:

```text
https://bit.ly/mprepo
```

Тот же файл: [plugins/repo.json](../plugins/repo.json).

Для использования данного альтернативного репозитория плагинов прописать в настройках Movian адрес выше.

Путь: **Settings → General → Alternative plugin repository URL**
(Настройки → Общие → Alternative plugin repository URL).

Вернуться на домашний экран. Плагины появятся в **Plugins → Browse available plugins**.

P.S.
Если вдруг в альтернативном репозитории категории остаются пустыми или плагины в них исчезли, либо при установке плагинов выдает ошибку, то пробуем использовать сервисы VPN.

Чтобы вернуться к официальному репозиторию, прописанный в настройках Movian адрес альтернативного репозитория полностью удаляем, оставив поле пустым.

На домашнем экране PS5 тот же каталог открывает строка Plugins. Без файла настроек адрес уже `https://bit.ly/mprepo`. Пустое сохранённое поле — официальный репозиторий. Вправо на строке репозитория переключает готовые адреса. Консоль не ходит по редиректам bit.ly. Если этот адрес не открывается, следующий в том же переключении — прямой файл на GitHub.

## English

Enter this address in Movian:

```text
https://bit.ly/mprepo
```

**Settings → General → Alternative plugin repository URL**

Go back to the home screen. Plugins are listed under **Plugins → Browse available plugins**.

If categories stay empty, plugins disappear, or install fails, try a VPN.

To use the official repository again, delete the address and leave the field empty.

## On this PS5 title

**Phase 0 does not run plugins.** The Plugins tab is a placeholder. The feed
at [`plugins/repo.json`](../plugins/repo.json) (https://bit.ly/mprepo) is kept
so Phase 4 can load the same catalog M7 uses.

Target behaviour (Phase 4), matching M7 / movian-go:

- Settings store an alternate repository URL; empty means
  `https://repo.movian.eu/plugins-v1.json`.
- Install zips into `/download0/plugins` (and USB `plugins/` if visible).
- `plugin.json` API v2: `createService`, `addURI`, `page.appendItem`.

See [PRODUCT.md](PRODUCT.md) and [PORTING_PLAN.md](PORTING_PLAN.md).
