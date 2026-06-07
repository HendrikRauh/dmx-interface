const REPO = "HendrikRauh/dmx-interface";

const radioGithub = document.getElementById("mode-github");
const radioLocal = document.getElementById("mode-local");
const secGithub = document.getElementById("sec-github");
const secLocal = document.getElementById("sec-local");
const select = document.getElementById("release-select");
const fileInput = document.getElementById("file-input");
const container = document.getElementById("flash-container");

function renderButton(binDataUrl) {
  if (!binDataUrl) {
    container.innerHTML = "";
    container.classList.add("hidden");
    return;
  }

  const manifestObject = {
    name: "ChaosDMX",
    version: "Dynamic Build",
    builds: [
      {
        chipFamily: "ESP32-S2",
        parts: [{ path: binDataUrl, offset: 0 }],
      },
    ],
  };

  const jsonString = JSON.stringify(manifestObject);
  const base64Manifest = btoa(
    encodeURIComponent(jsonString).replace(/%([0-9A-F]{2})/g, (match, p1) => {
      return String.fromCharCode("0x" + p1);
    })
  );
  const dataUri = `data:application/json;base64,${base64Manifest}`;

  const newButton = document.createElement("esp-web-install-button");
  newButton.setAttribute("manifest", dataUri);

  container.innerHTML = "";
  container.appendChild(newButton);
  container.classList.remove("hidden");
}

function updateUI() {
  if (radioGithub.checked) {
    secGithub.classList.remove("hidden");
    secLocal.classList.add("hidden");
    if (select.value && !["error", "loading"].includes(select.value)) {
      renderButton(select.value);
    } else {
      renderButton(null);
    }
  } else {
    secGithub.classList.add("hidden");
    secLocal.classList.remove("hidden");
    handleLocalFile();
  }
}

radioGithub.addEventListener("change", updateUI);
radioLocal.addEventListener("change", updateUI);

async function loadGitHubReleases() {
  try {
    const response = await fetch("meta/releases.json");
    if (!response.ok)
      throw new Error(`Error loading releases: ${response.status}`);

    const releases = await response.json();
    select.innerHTML = "";
    let hasAssets = false;

    if (Array.isArray(releases) && releases.length > 0) {
      releases.forEach(release => {
        release.assets.forEach(asset => {
          const option = document.createElement("option");
          option.text = `${release.tag} — ${asset.name}`;
          option.value = asset.url;
          select.appendChild(option);
          hasAssets = true;
        });
      });
    }

    if (hasAssets) {
      updateUI();
    } else {
      select.innerHTML =
        '<option value="error">No .bin assets found in releases.json</option>';
      renderButton(null);
    }
  } catch (err) {
    console.error(err);
    select.innerHTML =
      '<option value="error">Error loading local releases.json</option>';
    renderButton(null);
  }
}

select.addEventListener("change", () => {
  if (select.value && select.value !== "error") {
    renderButton(select.value);
  }
});

function handleLocalFile() {
  const file = fileInput.files[0];
  if (!file) {
    renderButton(null);
    return;
  }

  const reader = new FileReader();
  reader.onload = function (e) {
    renderButton(e.target.result);
  };
  reader.readAsDataURL(file);
}

fileInput.addEventListener("change", handleLocalFile);

loadGitHubReleases();
