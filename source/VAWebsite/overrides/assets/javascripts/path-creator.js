var paths = document.getElementById("paths");
const padding = 4;

var bezierWeight = 0.675;

var connections = [{
    boxA: "#VA",
    boxB: "#RAVEN"
}, {
    boxA: "#VA",
    boxB: "#GA"
}];

const coordinates = function () {

    let oldPaths = paths.children;

    for (let a = oldPaths.length - 1; a >= 0; a--) {
        paths.removeChild(oldPaths[a]);
    }

    let x1, y1, x4, y4, dx, x2, x3, path, boxA, boxB;

    for (let a = 0; a < connections.length; a++) {
        boxA = $(connections[a].boxA);
        boxB = $(connections[a].boxB);

        x1 = boxA.offset().left;
        y1 = boxA.offset().top + boxA.height() / 2;
        x4 = boxB.offset().left + boxB.width() / 2;
        y4 = boxB.offset().top;

        const rad = 30;
        const d1 = 100;
        const d2 = Math.min(x1 / 2, d1);
        const d3 = Math.min(x1 / 2, d1 / 4);

        data = `
                M${x1 + 5} ${y1} 
                l -5 0
                l ${-d2 + rad}  0
                a ${rad} ${rad} 0 0 0 -${rad} ${rad}
                V ${y4 - 2 * rad - d3}
                a ${rad} ${rad} 0 0 0 ${rad} ${rad}
                H ${x4 - rad}
                a ${rad} ${rad} 0 0 1 ${rad} ${rad}
                V ${y4 + 5}
                `;
        path = document.createElementNS("http://www.w3.org/2000/svg", "path");
        path.setAttribute("d", data);
        path.setAttribute("class", "path");
        paths.appendChild(path);
    }
}

$(window).load(function () {
    coordinates();
})

$(window).resize(function () {
    coordinates();
});