const configContaner = document.getElementById("config");
const clientContaner = document.getElementById("client");

window.addEventListener('popstate', onLocationChange);

function onLocationChange() {
  var route = window.location.hash;
  if (route === "") {
    route = "#/config";
    window.location.href = route;
  }

  if (route === "#/config") {
    clientContaner.style.display = "none";
    configContaner.style.display = "block";
  } else if (route === "#/client") {
    configContaner.style.display = "none";
    clientContaner.style.display = "block";
  }
}

onLocationChange();