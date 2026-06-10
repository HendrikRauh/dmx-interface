import {
  ESPLoader,
  Transport,
  HardReset,
  ClassicReset
} from "https://unpkg.com/esptool-js/bundle.js";

const themeToggle = document.getElementById("theme-toggle");
const body = document.body;

const radioGithub = document.getElementById("mode-github");
const radioLocal = document.getElementById("mode-local");
const secGithub = document.getElementById("sec-github");
const secLocal = document.getElementById("sec-local");
const releaseSelect = document.getElementById("release-select");
const fileInput = document.getElementById("file-input");
const dropZone = document.getElementById("drop-zone");
const dropText = document.getElementById("drop-text");
const flashSizeSelect = document.getElementById("flash-size");

const connectBtn = document.getElementById("connect-button");
const rebootBtn = document.getElementById("reboot-button");
const flashBtn = document.getElementById("flash-button");
const statusMsg = document.getElementById("status");

const progressContainer = document.getElementById("progress-container");
const progressBar = document.getElementById("progress-bar");
const progressText = document.getElementById("progress-text");

const terminalContainer = document.getElementById("terminal-container");
const terminal = document.getElementById("terminal");
const clearTerminalBtn = document.getElementById("clear-terminal");

let port = null;
let transport = null;
let esploader = null;
let isMonitoring = false;
let monitorReader = null;

// --- Theme Logic ---
const setTheme = (theme) => {
  if (theme === "latte") {
    body.classList.replace("mocha", "latte");
    themeToggle.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="4"/><path d="M12 2v2"/><path d="M12 20v2"/><path d="m4.93 4.93 1.41 1.41"/><path d="m17.66 17.66 1.41 1.41"/><path d="M2 12h2"/><path d="M20 12h2"/><path d="m6.34 17.66-1.41 1.41"/><path d="m19.07 4.93-1.41 1.41"/></svg>';
  } else {
    body.classList.replace("latte", "mocha");
    themeToggle.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 3a6 6 0 0 0 9 9 9 9 0 1 1-9-9Z"/></svg>';
  }
  localStorage.setItem("theme", theme);
};

themeToggle.addEventListener("click", () => {
  const newTheme = body.classList.contains("mocha") ? "latte" : "mocha";
  setTheme(newTheme);
});

const savedTheme = localStorage.getItem("theme") || "mocha";
setTheme(savedTheme);

// --- Terminal Logic ---
const cleanTerminal = () => {
  terminal.textContent = "";
};

const writeToTerminal = (data) => {
  terminal.textContent += data;
  if (terminal.textContent.length > 50000) {
    terminal.textContent = terminal.textContent.slice(-40000);
  }
  terminal.scrollTop = terminal.scrollHeight;
};

clearTerminalBtn.addEventListener("click", cleanTerminal);

const espTerminal = {
  clean: cleanTerminal,
  writeLine: (data) => writeToTerminal(data + "\n"),
  write: (data) => writeToTerminal(data),
};

// --- Serial Monitor Logic ---
async function startMonitoring() {
  if (isMonitoring || !port) return;
  isMonitoring = true;
  const decoder = new TextDecoder();

  try {
    while (isMonitoring && port.readable) {
      if (port.readable.locked) {
        await new Promise(resolve => setTimeout(resolve, 100));
        continue;
      }
      
      monitorReader = port.readable.getReader();
      try {
        while (true) {
          const { value, done } = await monitorReader.read();
          if (done) break;
          if (value) {
            writeToTerminal(decoder.decode(value));
          }
        }
      } catch (err) {
        if (err.name !== "LockLostError" && isMonitoring) {
          console.error("Monitor read error:", err);
        }
      } finally {
        if (monitorReader) {
          monitorReader.releaseLock();
          monitorReader = null;
        }
      }
      
      if (!isMonitoring) break;
      await new Promise(resolve => setTimeout(resolve, 100));
    }
  } catch (err) {
    if (isMonitoring) console.error("Monitor loop error:", err);
  }
}

function stopMonitoring() {
  isMonitoring = false;
  if (monitorReader) {
    try {
      monitorReader.cancel();
    } catch (e) {}
  }
}

// --- UI Helpers ---
const updateStatus = (msg, isError = false) => {
  statusMsg.textContent = msg;
  statusMsg.style.color = isError ? "#f38ba8" : "var(--text-muted)";
  if (!isError && body.classList.contains("latte")) {
    statusMsg.style.color = "var(--text-muted)";
  }
};

const setProgress = (percent) => {
  progressContainer.classList.remove("hidden");
  progressBar.style.width = `${percent}%`;
  progressText.textContent = `${Math.round(percent)}%`;
};

const toggleFlashButton = () => {
  const hasFile = radioGithub.checked 
    ? (releaseSelect.value && !["error", "loading"].includes(releaseSelect.value))
    : (fileInput.files.length > 0);
  
  const isConnected = !!port;
  
  flashBtn.disabled = !hasFile || !isConnected;
  rebootBtn.disabled = !isConnected;

  isConnected ? rebootBtn.classList.remove("hidden") : rebootBtn.classList.add("hidden");
  (hasFile && isConnected) ? flashBtn.classList.remove("hidden") : flashBtn.classList.add("hidden");
};

// --- Event Listeners ---
radioGithub.addEventListener("change", () => {
  secGithub.classList.remove("hidden");
  secLocal.classList.add("hidden");
  toggleFlashButton();
});

radioLocal.addEventListener("change", () => {
  secGithub.classList.add("hidden");
  secLocal.classList.remove("hidden");
  toggleFlashButton();
});

releaseSelect.addEventListener("change", toggleFlashButton);

// --- Drop Zone Logic ---
dropZone.addEventListener("click", () => fileInput.click());

dropZone.addEventListener("dragover", (e) => {
  e.preventDefault();
  dropZone.classList.add("active");
});

["dragleave", "dragend"].forEach((type) => {
  dropZone.addEventListener(type, () => {
    dropZone.classList.remove("active");
  });
});

dropZone.addEventListener("drop", (e) => {
  e.preventDefault();
  dropZone.classList.remove("active");
  if (e.dataTransfer.files.length) {
    fileInput.files = e.dataTransfer.files;
    handleFileSelect();
  }
});

const handleFileSelect = () => {
  if (fileInput.files.length > 0) {
    dropText.textContent = fileInput.files[0].name;
    dropText.style.color = "var(--teal)";
  } else {
    dropText.textContent = "Click or drag .bin file here";
    dropText.style.color = "var(--text-muted)";
  }
  toggleFlashButton();
};

fileInput.addEventListener("change", handleFileSelect);

// --- GitHub Release Loading ---
async function loadGitHubReleases() {
  try {
    const response = await fetch("meta/releases.json");
    if (!response.ok) throw new Error("Failed to load releases.json");
    
    const releases = await response.json();
    releaseSelect.innerHTML = "";
    
    let hasAssets = false;
    if (Array.isArray(releases)) {
      releases.forEach(release => {
        release.assets.forEach(asset => {
          if (asset.name.endsWith(".bin")) {
            const option = document.createElement("option");
            option.text = `${release.tag} — ${asset.name}`;
            option.value = asset.url;
            releaseSelect.appendChild(option);
            hasAssets = true;
          }
        });
      });
    }

    if (!hasAssets) {
      releaseSelect.innerHTML = '<option value="error">No firmware found</option>';
    }
    toggleFlashButton();
  } catch (err) {
    console.error(err);
    releaseSelect.innerHTML = '<option value="error">Error loading releases</option>';
  }
}

// --- Connection Logic ---
connectBtn.addEventListener("click", async () => {
  if (port) {
    stopMonitoring();
    try {
      if (transport) await transport.disconnect();
      await port.close();
    } catch (e) {}
    port = null;
    transport = null;
    esploader = null;
    connectBtn.textContent = "Connect Device";
    connectBtn.classList.replace("btn-teal", "btn-primary");
    updateStatus("Disconnected");
    flashBtn.classList.add("hidden");
    rebootBtn.classList.add("hidden");
    terminalContainer.classList.add("hidden");
    toggleFlashButton();
    return;
  }

  try {
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });
    
    // Create transport and loader for potential flashing
    transport = new Transport(port, true);
    const resetConstructors = {
      hardReset: (transport, usingUsbOtg) => new HardReset(transport, usingUsbOtg),
      classicReset: (transport, resetDelay) => new ClassicReset(transport, resetDelay),
    };
    esploader = new ESPLoader({
      transport: transport,
      baudrate: 115200,
      terminal: espTerminal,
      resetConstructors: resetConstructors
    });

    updateStatus("Connected");
    terminalContainer.classList.remove("hidden");
    connectBtn.textContent = "Disconnect";
    connectBtn.classList.replace("btn-primary", "btn-teal");
    toggleFlashButton();
    
    startMonitoring();
  } catch (err) {
    console.error(err);
    updateStatus(`Connection failed: ${err.message}`, true);
    port = null;
  }
});

// --- Reboot Logic ---
rebootBtn.addEventListener("click", async () => {
  if (!port) return;
  try {
    rebootBtn.disabled = true;
    connectBtn.disabled = true;
    flashBtn.disabled = true;

    stopMonitoring();
    await new Promise(resolve => setTimeout(resolve, 300));

    updateStatus("Rebooting...");
    
    // Manual RTS toggle is often more reliable than software reset command
    // when the application is flooding the port
    try {
      await port.setSignals({ requestToSend: true }); // EN Low
      await new Promise(r => setTimeout(r, 100));
      await port.setSignals({ requestToSend: false }); // EN High
    } catch (e) {
      // Fallback to esptool method
      await esploader.after("hard_reset", true);
    }
    
    await new Promise(resolve => setTimeout(resolve, 800));
    updateStatus("Rebooted!");
    startMonitoring();
    
  } catch (err) {
    console.error(err);
    updateStatus(`Reboot failed: ${err.message}`, true);
  } finally {
    rebootBtn.disabled = false;
    connectBtn.disabled = false;
    toggleFlashButton();
  }
});

// --- Flashing Logic ---
flashBtn.addEventListener("click", async () => {
  if (!esploader || !port) return;

  try {
    flashBtn.disabled = true;
    connectBtn.disabled = true;
    rebootBtn.disabled = true;

    stopMonitoring();
    await new Promise(resolve => setTimeout(resolve, 500));

    updateStatus("Syncing with bootloader...");
    
    // Disconnect transport if it was opened manually for monitoring.
    // esptool-js wants to handle the opening itself to set correct baud rates.
    try {
      if (transport) await transport.disconnect();
    } catch (e) {}

    // Now try to sync. This will handle the reset into bootloader mode.
    const chip = await esploader.main();
    updateStatus(`Preparing firmware for ${chip}...`);
    setProgress(0);

    let binData;
    if (radioGithub.checked) {
      const url = releaseSelect.value;
      try {
        const response = await fetch(url);
        if (!response.ok) throw new Error("Direct fetch failed");
        binData = new Uint8Array(await response.arrayBuffer());
      } catch (e) {
        updateStatus("Direct fetch failed, trying proxy...", false);
        const proxyUrl = `https://cors-anywhere.herokuapp.com/${url}`;
        const response = await fetch(proxyUrl);
        if (!response.ok) throw new Error("CORS Proxy also failed.");
        binData = new Uint8Array(await response.arrayBuffer());
      }
    } else {
      const file = fileInput.files[0];
      binData = new Uint8Array(await file.arrayBuffer());
    }

    updateStatus("Flashing...");
    
    const flashOptions = {
      fileArray: [{ data: binData, address: 0x0000 }],
      flashMode: "dio",
      flashFreq: "40m",
      flashSize: flashSizeSelect.value,
      eraseAll: false,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        setProgress((written / total) * 100);
      },
    };

    await esploader.writeFlash(flashOptions);
    updateStatus("Flashing complete!");
    setProgress(100);
    
    await esploader.after("hard_reset", true);
    await new Promise(resolve => setTimeout(resolve, 800));
    
    startMonitoring();
    
  } catch (err) {
    console.error(err);
    updateStatus(`Error: ${err.message}`, true);
  } finally {
    flashBtn.disabled = false;
    connectBtn.disabled = false;
    toggleFlashButton();
  }
});

loadGitHubReleases();
