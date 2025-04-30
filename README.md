# 💰 Kaun Banega Patchaspati (KBp) - Terminal Edition

A fun and interactive *KBC-style* quiz game written in C++ for the terminal! It features:

- 🎲 Random trivia questions from the OpenTDB API
- 💡 Lifelines: Ask a Friend and 50:50
- 💵 Money ladder with increasing rewards
- ⏱️ Timed questions (60 seconds)
- 🔊 Intro sound on game start (Windows only)

---

## 🚀 Features

- Uses **libcurl** to fetch questions from Open Trivia DB.
- Parses JSON using **nlohmann/json**.
- Uses **Windows multimedia API** to play intro sound.
- Includes a **manual Stack and Queue** implementation.
- Clean and interactive **console UI**.

---

## 📁 Folder Structure
KBP
 |
 |------>main.cpp
 |------>start.wav
 |------>main.exe(after compilation)


> ⚠️ Ensure `intro.wav` is present in the **same folder** as the executable.

---

## 🔧 Compilation Instructions (Windows)

**Requirements**:
- g++ compiler (via MSYS2 or MinGW)
- Internet connection (to fetch questions)

### ✅ Compile with:


```bash
g++ main.cpp -o main.exe -I"C:/vcpkg-master/installed/x64-windows/include" -L"C:/vcpkg-master/installed/x64-windows/lib" -lcurl -lwinmm


