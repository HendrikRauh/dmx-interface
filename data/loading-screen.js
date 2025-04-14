const content = document.querySelector("section.content");
const loadingScreen = document.querySelector(".loading-screen");
const loadingMsg = loadingScreen.querySelector("h2");
const spinner = loadingScreen.querySelector(".spinner");
const reloadBtn = loadingScreen.querySelector(".reload");

export function showLoadingScreen(msg) {
    hide(content, reloadBtn);
    show(loadingScreen, spinner);
    loadingMsg.classList.remove("error");
    loadingMsg.textContent = msg;
}

export function showError(msg) {
    showLoadingScreen(msg);
    loadingMsg.innerHTML +=
        "<br/>Stelle sicher, dass du mit dem DMX-Interface verbunden bist und die IP-Adresse stimmt.";
    show(reloadBtn);
    hide(spinner);
    loadingMsg.classList.add("error");
}

export function hideLoadingScreen() {
    hide(loadingScreen, reloadBtn);
    show(content);
    loadingMsg.classList.remove("error");
    loadingMsg.textContent = "";
}

function show(...elements) {
    for (const element of elements) {
        element.classList.remove("hidden");
    }
}

function hide(...elements) {
    for (const element of elements) {
        element.classList.add("hidden");
    }
}
