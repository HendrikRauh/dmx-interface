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

    putData(data);
});

async function putData(data) {
    try {
        const res = await fetch("/config", {
            method: "PUT",
            headers: {
                "Content-Type": "application/json",
            },
            body: JSON.stringify(data),
        });
        if (!res.ok) {
            throw new Error(`Response status: ${res.status}`);
        }

        const json = await res.json();
        console.log(json);
    } catch (error) {
        console.error(error.message);
    }
}
