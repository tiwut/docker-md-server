# Tiwut Docker Markdown Server

<img width="837" height="676" alt="image" src="https://github.com/user-attachments/assets/a8f56c0a-f8a6-4934-9091-1889577a785c" />


```yaml
version: '3.8'

services:
  md-server:
    image: tiwutdev/docker-md-server:latest
    ports:
      - "8080:8080"
      - "8081:8081"
    volumes:
      - ./www:/app/www
    restart: unless-stopped
```

A lightweight, 100% C++ written web server for hosting Markdown files, featuring an integrated web file manager.

## Architecture
- **Pure C++17:** Zero external dependencies (only requires libc/libstdc++).
- **Public Port (8080):** Renders `.md` files dynamically in the browser using `marked.js`. Supports custom CSS and JS.
- **Admin Port (8081):** Full web UI for file management (editor, upload, folder creation, rename, delete, copy).

## Setup & Run

1. Build and start the container using Docker Compose:
```bash
   docker-compose up -d --build
```


- **Accessing the Server:**

- **Frontend:** [http://localhost:8080](https://www.google.com/search?q=http://localhost:8080)
- **Admin UI:** [http://localhost:8081](https://www.google.com/search?q=http://localhost:8081)

## File Structure

All files are saved in the local directory ./www, mounted into the container via Docker volumes.

- index.md: The main page.
- style.css: Styling (dark mode design included).
- script.js: Your custom JavaScript.

## Admin Panel Features

- **Live Editor:** Edit Markdown, CSS, and JS files directly in the browser.
- **Hotkeys:** Save changes quickly using Ctrl + S.
- **Upload:** Use the upload button to easily deploy assets like images or PDFs.
- **Security:** Path sanitization blocks directory traversal attacks (../).

