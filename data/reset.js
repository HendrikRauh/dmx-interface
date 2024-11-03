import { writeDataToInput } from "/load-data.js";

const form = document.querySelector("form");

form.addEventListener("reset", async (event) => {
    event.preventDefault();

    try {
        const res = await fetch("/reset", {
            method: "POST",
        });
        if (!res.ok) {
            throw new Error(`Response status: ${res.status}`);
        }

        const json = await res.json();
        writeDataToInput(json);
    } catch (error) {
        console.error(error.message);
    }
});
