---
title: Web UI
description: The embedded web UI — stack, dev workflow with mocks.
slug: web
---

## Stack :badge[SOON]{variant=note}

## Working on the UI

You do not need a device to work on the UI: the dev server mocks
`GET/POST /api/config`, so the form behaves as it would against the real
firmware.

```sh
inv web:dev     # dev server with mock API (web/mock/config.mock.ts)
inv web:build   # production build → web/dist/
```
