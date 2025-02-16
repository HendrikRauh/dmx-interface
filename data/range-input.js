const form = document.querySelector("form");

form.addEventListener("input", (event) => {
    if (event.target.classList.contains("range")) {
        updateValue(event.target);
    }
});

form.addEventListener("change", () => {
    console.log("received change event");
    document.querySelectorAll("input[type='range']").forEach((input) => {
        updateValue(input);
    });
});

function updateValue(slider) {
    console.log("update slide value");
    const percentage = Math.round((slider.value / slider.max) * 100);
    slider.nextElementSibling.innerText = `${percentage}%`;
}
