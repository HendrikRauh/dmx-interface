import { writeDataToInput } from "/load-data.js";

const form = document.querySelector("form");

form.addEventListener("reset", async (event) => {
    event.preventDefault();

    const ok = confirm(
        "Sicher, dass du alle Einstellungen zurücksetzen möchtest?"
    );
    if (ok) {
        reset();
    }
});

async function reset() {
    try {
        const res = await fetch("/config", {
            method: "DELETE",
        });
        if (!res.ok) {
            throw new Error(`Response status: ${res.status}`);
        }

        const json = await res.json();
        writeDataToInput(json);
    } catch (error) {
        console.error(error.message);
    }
}
