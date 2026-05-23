#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;
const std::string WWW_DIR = "./www";

std::string sanitize(std::string path) {
    if (path.find("..") != std::string::npos) return "";
    while (!path.empty() && path[0] == '/') path = path.substr(1);
    return path;
}

std::string get_header(const std::string& req, const std::string& header) {
    size_t pos = req.find(header + ": ");
    if (pos == std::string::npos) return "";
    size_t end = req.find("\r\n", pos);
    return req.substr(pos + header.length() + 2, end - pos - header.length() - 2);
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    file << content;
}

bool ends_with(std::string_view str, std::string_view suffix) {
    return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
}

std::string read_request(int client_sock) {
    char buffer[4096];
    std::string req;
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        req.append(buffer);
        size_t cl_pos = req.find("Content-Length: ");
        if (cl_pos != std::string::npos) {
            size_t cl_end = req.find("\r\n", cl_pos);
            int length = std::stoi(req.substr(cl_pos + 16, cl_end - cl_pos - 16));
            size_t header_end = req.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                int current_body = req.length() - (header_end + 4);
                int remaining = length - current_body;
                while (remaining > 0) {
                    bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                    if (bytes <= 0) break;
                    buffer[bytes] = '\0';
                    req.append(buffer);
                    remaining -= bytes;
                }
            }
        }
    }
    return req;
}

void send_res(int sock, const std::string& type, const std::string& body) {
    std::string res = "HTTP/1.1 200 OK\r\nContent-Type: " + type + "\r\nContent-Length: " + 
                      std::to_string(body.length()) + "\r\nConnection: close\r\n\r\n" + body;
    send(sock, res.c_str(), res.length(), 0);
}

void handle_public(int client_sock) {
    std::string req = read_request(client_sock);
    if (req.empty()) { close(client_sock); return; }

    std::istringstream stream(req);
    std::string method, path;
    stream >> method >> path;

    if (path == "/") path = "/index.md";
    std::string filepath = WWW_DIR + "/" + sanitize(path);

    if (!fs::exists(filepath) || fs::is_directory(filepath)) {
        std::string error = "HTTP/1.1 404 Not Found\r\n\r\n404";
        send(client_sock, error.c_str(), error.length(), 0);
    } else if (ends_with(path, ".md")) {
        std::string md_content = read_file(filepath);
        std::string html = R"HTML(<!DOCTYPE html><html><head>
            <meta charset="UTF-8">
            <link rel="stylesheet" href="/style.css">
            <script src="https://cdn.jsdelivr.net/npm/marked/marked.min.js"></script>
            </head><body>
            <textarea id="raw-md" style="display:none;">)HTML" + md_content + R"HTML(</textarea>
            <div id="content"></div>
            <script>document.getElementById('content').innerHTML = marked.parse(document.getElementById('raw-md').value);</script>
            <script src="/script.js"></script>
            </body></html>)HTML";
        send_res(client_sock, "text/html; charset=utf-8", html);
    } else if (ends_with(path, ".css")) {
        send_res(client_sock, "text/css; charset=utf-8", read_file(filepath));
    } else if (ends_with(path, ".js")) {
        send_res(client_sock, "application/javascript; charset=utf-8", read_file(filepath));
    } else {
        send_res(client_sock, "application/octet-stream", read_file(filepath));
    }
    close(client_sock);
}

void handle_admin(int client_sock) {
    std::string req = read_request(client_sock);
    if (req.empty()) { close(client_sock); return; }

    std::istringstream stream(req);
    std::string method, path;
    stream >> method >> path;

    if (method == "GET" && path == "/") {
        std::string admin_html = R"HTML(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Tiwut Server Manager</title>
    <style>
        :root { --bg: #0f172a; --panel: #1e293b; --text: #f8fafc; --accent: #38bdf8; --border: #334155; --hover: #334155; --danger: #ef4444; }
        body { font-family: system-ui, sans-serif; background: var(--bg); color: var(--text); margin: 0; display: flex; height: 100vh; overflow: hidden; }
        .sidebar { width: 300px; background: var(--panel); border-right: 1px solid var(--border); display: flex; flex-direction: column; }
        .sidebar-header { padding: 1rem; border-bottom: 1px solid var(--border); display: flex; flex-wrap: wrap; gap: 0.5rem; }
        .btn { background: var(--border); color: var(--text); border: none; padding: 0.5rem; border-radius: 4px; cursor: pointer; font-size: 0.85rem; flex-grow: 1; }
        .btn:hover { background: var(--accent); color: #000; }
        .btn.danger:hover { background: var(--danger); color: #fff; }
        .file-list { flex-grow: 1; overflow-y: auto; padding: 0.5rem; }
        .file-item { padding: 0.5rem; border-radius: 4px; cursor: pointer; display: flex; justify-content: space-between; align-items: center; }
        .file-item:hover { background: var(--hover); }
        .file-item.active { background: var(--accent); color: #000; }
        .main { flex-grow: 1; display: flex; flex-direction: column; }
        .editor-header { padding: 1rem; border-bottom: 1px solid var(--border); background: var(--panel); display: flex; justify-content: space-between; align-items: center; }
        textarea { flex-grow: 1; background: var(--bg); color: var(--text); border: none; padding: 1rem; font-family: ui-monospace, monospace; resize: none; outline: none; font-size: 0.95rem; line-height: 1.5; }
        #upload-input { display: none; }
        .path-nav { font-size: 0.9rem; color: var(--accent); cursor: pointer; }
    </style>
</head>
<body>
    <div class="sidebar">
        <div class="sidebar-header">
            <span class="path-nav" onclick="navUp()">&#11013; Back</span>
            <span id="current-dir" style="width:100%; font-size:0.8rem; color:#94a3b8;">/</span>
            <button class="btn" onclick="createFile()">+ File</button>
            <button class="btn" onclick="createFolder()">+ Folder</button>
            <button class="btn" onclick="document.getElementById('upload-input').click()">Upload</button>
            <input type="file" id="upload-input" onchange="uploadFile(event)">
        </div>
        <div class="file-list" id="file-list"></div>
    </div>
    <div class="main">
        <div class="editor-header">
            <span id="active-file">No file selected</span>
            <div>
                <button class="btn" onclick="renameFile()">Rename</button>
                <button class="btn" onclick="copyFile()">Copy</button>
                <button class="btn danger" onclick="deleteFile()">Delete</button>
                <button class="btn" onclick="saveFile()" style="margin-left:1rem; background:var(--accent); color:#000;">Save (Ctrl+S)</button>
            </div>
        </div>
        <textarea id="editor" spellcheck="false" disabled></textarea>
    </div>

    <script>
        let currentDir = "";
        let currentFile = "";

        async function apiCall(endpoint, method, headers, body = null) {
            let res = await fetch(endpoint, { method, headers, body });
            if (!res.ok) alert("Error: " + res.statusText);
            return res;
        }

        async function loadFiles() {
            let res = await apiCall('/api/list', 'GET', { 'X-Dir': currentDir });
            let files = await res.json();
            let html = '';
            files.sort((a,b) => b.is_dir - a.is_dir || a.name.localeCompare(b.name));
            files.forEach(f => {
                let icon = f.is_dir ? "📁 " : "📄 ";
                let path = currentDir ? currentDir + "/" + f.name : f.name;
                let onclick = f.is_dir ? `openDir('${path}')` : `openFile('${path}')`;
                html += `<div class="file-item ${currentFile===path?'active':''}" onclick="${onclick}">
                            <span>${icon} ${f.name}</span>
                         </div>`;
            });
            document.getElementById('file-list').innerHTML = html;
            document.getElementById('current-dir').innerText = "/" + currentDir;
        }

        function openDir(path) { currentDir = path; loadFiles(); }
        function navUp() { 
            let parts = currentDir.split('/'); parts.pop(); 
            currentDir = parts.join('/'); loadFiles(); 
        }

        async function openFile(path) {
            currentFile = path;
            let res = await apiCall('/api/load', 'GET', { 'X-File': path });
            document.getElementById('editor').value = await res.text();
            document.getElementById('editor').disabled = false;
            document.getElementById('active-file').innerText = path;
            loadFiles();
        }

        async function saveFile() {
            if(!currentFile) return;
            let txt = document.getElementById('editor').value;
            await apiCall('/api/save', 'POST', { 'X-File': currentFile }, txt);
        }

        async function createFile() {
            let name = prompt("Filename:");
            if(!name) return;
            let path = currentDir ? currentDir + "/" + name : name;
            await apiCall('/api/save', 'POST', { 'X-File': path }, "");
            openFile(path);
        }

        async function createFolder() {
            let name = prompt("Folder name:");
            if(!name) return;
            let path = currentDir ? currentDir + "/" + name : name;
            await apiCall('/api/mkdir', 'POST', { 'X-Dir': path });
            loadFiles();
        }

        async function uploadFile(e) {
            let file = e.target.files[0];
            if(!file) return;
            let path = currentDir ? currentDir + "/" + file.name : file.name;
            let reader = new FileReader();
            reader.onload = async (ev) => {
                await apiCall('/api/save', 'POST', { 'X-File': path }, ev.target.result);
                loadFiles();
            };
            reader.readAsArrayBuffer(file);
        }

        async function deleteFile() {
            if(!currentFile || !confirm(`Delete ${currentFile}?`)) return;
            await apiCall('/api/delete', 'POST', { 'X-File': currentFile });
            currentFile = ""; document.getElementById('editor').value = "";
            document.getElementById('editor').disabled = true;
            document.getElementById('active-file').innerText = "No file selected";
            loadFiles();
        }

        async function renameFile() {
            if(!currentFile) return;
            let newName = prompt("New name including path:", currentFile);
            if(!newName || newName === currentFile) return;
            await apiCall('/api/rename', 'POST', { 'X-File': currentFile, 'X-New': newName });
            openFile(newName);
        }

        async function copyFile() {
            if(!currentFile) return;
            let newName = prompt("Copy to:", currentFile + "_copy");
            if(!newName) return;
            await apiCall('/api/copy', 'POST', { 'X-File': currentFile, 'X-New': newName });
            loadFiles();
        }

        document.addEventListener('keydown', e => {
            if (e.ctrlKey && e.key === 's') { e.preventDefault(); saveFile(); }
        });

        loadFiles();
    </script>
</body>
</html>)HTML";
        send_res(client_sock, "text/html; charset=utf-8", admin_html);
    } 
    else if (path == "/api/list") {
        std::string dir = sanitize(get_header(req, "X-Dir"));
        std::string full_path = WWW_DIR + (dir.empty() ? "" : "/" + dir);
        std::string json = "[";
        if (fs::exists(full_path) && fs::is_directory(full_path)) {
            bool first = true;
            for (const auto& entry : fs::directory_iterator(full_path)) {
                if (!first) json += ",";
                json += "{\"name\":\"" + entry.path().filename().string() + "\",\"is_dir\":" + std::string(entry.is_directory()?"true":"false") + "}";
                first = false;
            }
        }
        json += "]";
        send_res(client_sock, "application/json; charset=utf-8", json);
    }
    else if (path == "/api/load") {
        std::string file = sanitize(get_header(req, "X-File"));
        send_res(client_sock, "text/plain; charset=utf-8", read_file(WWW_DIR + "/" + file));
    } 
    else if (method == "POST" && path == "/api/save") {
        std::string file = sanitize(get_header(req, "X-File"));
        size_t body_pos = req.find("\r\n\r\n");
        if (!file.empty() && body_pos != std::string::npos) {
            write_file(WWW_DIR + "/" + file, req.substr(body_pos + 4));
            send_res(client_sock, "text/plain", "OK");
        }
    }
    else if (method == "POST" && path == "/api/mkdir") {
        std::string dir = sanitize(get_header(req, "X-Dir"));
        if (!dir.empty()) fs::create_directories(WWW_DIR + "/" + dir);
        send_res(client_sock, "text/plain", "OK");
    }
    else if (method == "POST" && path == "/api/delete") {
        std::string file = sanitize(get_header(req, "X-File"));
        if (!file.empty()) fs::remove_all(WWW_DIR + "/" + file);
        send_res(client_sock, "text/plain", "OK");
    }
    else if (method == "POST" && path == "/api/rename") {
        std::string old_f = sanitize(get_header(req, "X-File"));
        std::string new_f = sanitize(get_header(req, "X-New"));
        if (!old_f.empty() && !new_f.empty()) fs::rename(WWW_DIR + "/" + old_f, WWW_DIR + "/" + new_f);
        send_res(client_sock, "text/plain", "OK");
    }
    else if (method == "POST" && path == "/api/copy") {
        std::string old_f = sanitize(get_header(req, "X-File"));
        std::string new_f = sanitize(get_header(req, "X-New"));
        if (!old_f.empty() && !new_f.empty()) fs::copy(WWW_DIR + "/" + old_f, WWW_DIR + "/" + new_f, fs::copy_options::overwrite_existing);
        send_res(client_sock, "text/plain", "OK");
    }
    close(client_sock);
}

void run_server(int port, void(*handler)(int)) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);
    
    std::cout << "Server running on port " << port << std::endl;
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd >= 0) std::thread(handler, client_fd).detach();
    }
}

int main() {
    fs::create_directories(WWW_DIR);
    std::thread t1(run_server, 8080, handle_public);
    std::thread t2(run_server, 8081, handle_admin);
    t1.join();
    t2.join();
    return 0;
}