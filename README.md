# Chromium ML-Based Content Security Policy Enforcer

This project extends the Chromium browser to **detect and block malicious JavaScript (XSS attacks)** using a trained **machine learning model** (ONNX), and enforces **Content Security Policies (CSP)** dynamically.

---

## Objective

🔐 Prevent XSS attacks  
🧠 Use ML to identify malicious script content  
🧩 Allow clean parts of the page to load  
🚫 Block malicious tags and pages  
✅ Auto-inject CSP headers where required

---

## 📁 Modified Files

| File | Purpose |
|------|---------|
| `net/chrome_network_delegate.cc` | Intercepts responses at network layer |
| `net/url_request/url_loader.cc`  | Extracts HTML body and modifies response |
| `content/browser/xss_model_handler.cc` | Handles ONNX model inference and script scanning |
| `content/browser/xss_model_handler.h` | Header file for logic |
| `BUILD.gn` | Adds ONNX runtime + links model |
| `model/xss_model.onnx` | Your trained ML model |

---

## Core Logic

1. Intercept HTML response
2. Extract all `<script>` blocks
3. For each:
   - Predict using ONNX model
   - If **malicious** → remove the script
4. Load cleaned HTML
5. If anything was removed → inject strong CSP

---


## 🛠️ Build Instructions

1. Clone Chromium source (already built Chromium works too)
2. Copy modified files from `patch/` into your source:
   - Example:
     ```bash
     cp -r patch/net ~/chromium/src/net
     cp -r patch/content ~/chromium/src/content
     cp patch/BUILD.gn ~/chromium/src/BUILD.gn
     ```
3. Place `xss_model.onnx` inside your Chromium build at:
