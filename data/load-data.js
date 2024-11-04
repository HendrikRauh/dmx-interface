const form = document.querySelector("form");

async function loadData() {
    try {
        const req = await fetch("/config", {
            method: "GET",
        });
        if (!req.ok) {
            throw new Error(`Response status: ${req.status}`);
        }

        const json = await req.json();
        console.log(json);
        return json;
    } catch (error) {
        console.log(error);
        return null;
    }
}

export function writeDataToInput(data) {
    console.log("write data", typeof data);
    for (const [key, value] of Object.entries(data)) {
        const element = document.querySelector(`[name=${key}]`);
        console.log(key, element);

        if (element.type === "checkbox") {
            element.checked = value;
        } else {
            element.value = value;
        }
    }
    // send "change" event
    form.dispatchEvent(new Event("change", { bubbles: true }));
}

const data = await loadData();
if (data !== null) {
    writeDataToInput(data);
}
