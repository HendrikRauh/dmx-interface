import { updateConfig } from "/submit.js";

const form = document.querySelector("form.config");

form.addEventListener("reset", async (event) => {
    event.preventDefault();

    const ok = confirm(
        "Sicher, dass du alle Einstellungen zurücksetzen möchtest?"
    );
    if (ok) {
        updateConfig({
            method: "DELETE",
        });
    }
});
