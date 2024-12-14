document.querySelector("form").addEventListener("input", (event) => {
    if (event.target.classList.contains("range")) {
        updateValue(event.target);
    }
});

function updateValue(slider) {
    const percentage = Math.round((slider.value / slider.max) * 100);
    slider.nextElementSibling.innerText = `${percentage}%`;
}

document.querySelectorAll("input[type='range'].range").forEach((element) => {
    updateValue(element);
});
