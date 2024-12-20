const form = document.querySelector("form.config");
const dynamicInputs = form.querySelectorAll("[data-field][data-values]");

document.addEventListener("change", updateVisibility);

function updateVisibility() {
    dynamicInputs.forEach((element) => {
        const input = form.querySelector(`#${element.dataset.field}`);
        if (element.dataset.values.split("|").includes(input.value)) {
            element.classList.remove("hidden");
            element
                .querySelectorAll("input, select, button, textarea")
                .forEach((childInput) => (childInput.disabled = false));
        } else {
            element.classList.add("hidden");
            element
                .querySelectorAll("input, select, button, textarea")
                .forEach((childInput) => (childInput.disabled = true));
        }
    });
}

updateVisibility();
