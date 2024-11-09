import { loadData, writeDataToInput } from "./load-data.js";
import {
    showLoadingScreen,
    hideLoadingScreen,
    showError,
} from "./loading-screen.js";

const form = document.querySelector("form");

function parseValue(input) {
    if (input.type === "checkbox") {
        return input.checked
            ? input.dataset.valueChecked
            : input.dataset.valueNotChecked;
    }

    if (input.value === "") {
        return null;
    }

    if (input.type === "number") {
        const number = Number(input.value);
        return Number.isNaN(number) ? null : number;
    }

    return input.value;
}

form.addEventListener("submit", (event) => {
    event.preventDefault();
    const inputFields = document.querySelectorAll(
        "form :is(input, select, textarea):not(:disabled)"
    );

    const data = Array.from(inputFields).reduce((data, input) => {
        data[input.name] = parseValue(input);
        return data;
    }, {});
    console.log(data);

    updateConfig({
        method: "PUT",
        headers: {
            "Content-Type": "application/json",
        },
        body: JSON.stringify(data),
    });
});

export async function updateConfig(fetchOptions) {
    showLoadingScreen("Konfiguration anwenden und ESP neustarten...");

    try {
        const res = await fetch("/config", fetchOptions);
        if (!res.ok) {
            throw new Error(`Response status: ${res.status}`);
        }

        // wait for the esp to restart
        const delay = new Promise((resolve) =>
            setTimeout(() => resolve(), 500)
        );
        await delay;

        const data = await loadData(30 * 1000);
        writeDataToInput(data);
        hideLoadingScreen();
    } catch (error) {
        console.error(error.message);
        showError(error.message);
    }
}
