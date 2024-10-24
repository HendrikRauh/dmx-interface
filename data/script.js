const form = document.querySelector("form");
const dynamicInputs = form.querySelectorAll("[data-field][data-values]");

document.addEventListener("change", updateVisibility);

function updateVisibility() {
    dynamicInputs.forEach((element) => {
        const input = form.querySelector(`#${element.dataset.field}`);
        if (element.dataset.values.split("|").includes(input.value)) {
            element.classList.remove("hidden");
        } else {
            element.classList.add("hidden");
        }
    });
}

updateVisibility();
