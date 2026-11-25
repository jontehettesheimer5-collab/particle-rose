(() => {
  "use strict";

  const canvas = document.getElementById("rose");
  const context = canvas.getContext("2d", { alpha: false });
  const credit = document.getElementById("credit");
  const TAU = Math.PI * 2;
  const introDuration = 2.2;
  const cycleDuration = 11;
  const particles = [];
  let width = 0;
  let height = 0;
  let pixelRatio = 1;
  let startTime;
  let pauseTime;

  // A seeded generator keeps the same petal texture on every launch.
  let seed = 324;
  function random() {
    seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
    return seed / 4294967296;
  }

  function smoothstep(a, b, value) {
    const t = Math.max(0, Math.min(1, (value - a) / (b - a)));
    return t * t * (3 - 2 * t);
  }

  // Five overlapping whorls form a folded center and broad outer petals.
  const whorls = [
    { count: 5, radius: 0.16, length: 0.43, width: 0.23, lift: 0.65 },
    { count: 6, radius: 0.23, length: 0.61, width: 0.39, lift: 0.52 },
    { count: 7, radius: 0.32, length: 0.79, width: 0.53, lift: 0.36 },
    { count: 7, radius: 0.39, length: 0.99, width: 0.65, lift: 0.16 },
    { count: 6, radius: 0.43, length: 1.12, width: 0.72, lift: -0.05 }
  ];

  whorls.forEach((whorl, layer) => {
    for (let petal = 0; petal < whorl.count; petal += 1) {
      const angle = petal * TAU / whorl.count + layer * 2.39996;
      const variation = 0.93 + random() * 0.14;
      const count = layer < 2 ? 600 : 850;
      for (let index = 0; index < count; index += 1) {
        const u = random();
        const v = random() * 2 - 1;
        const spread = Math.pow(Math.sin(Math.PI * u), 0.63);
        const radial = whorl.radius * (1 - u) + whorl.length * u;
        const sideways = v * whorl.width * spread;
        const z = whorl.lift + Math.sin(u * Math.PI * 0.92) * 0.39
          - u * u * (0.47 + layer * 0.045)
          + Math.pow(Math.abs(v), 2.3) * 0.19 * spread;
        particles.push({
          x: (Math.cos(angle) * radial - Math.sin(angle) * sideways) * variation,
          y: (Math.sin(angle) * radial + Math.cos(angle) * sideways) * variation,
          z,
          layer,
          u,
          v,
          shade: random(),
          size: 0.8 + random() * 0.85,
          scatterX: (random() - 0.5) * 4.8,
          scatterY: (random() - 0.5) * 4.8,
          scatterZ: (random() - 0.5) * 3,
          phase: random() * TAU
        });
      }
    }
  });

  function resize() {
    width = window.innerWidth;
    height = window.innerHeight;
    pixelRatio = Math.min(window.devicePixelRatio || 1, 2);
    canvas.width = Math.round(width * pixelRatio);
    canvas.height = Math.round(height * pixelRatio);
    context.setTransform(pixelRatio, 0, 0, pixelRatio, 0, 0);
  }

  function render(timestamp) {
    if (startTime === undefined) startTime = timestamp;
    const elapsed = (timestamp - startTime) / 1000;
    context.fillStyle = "#000";
    context.fillRect(0, 0, width, height);

    if (elapsed < introDuration) {
      const fadeIn = smoothstep(0, 0.6, elapsed);
      const fadeOut = 1 - smoothstep(1.6, 2.2, elapsed);
      credit.style.opacity = String(fadeIn * fadeOut);
      requestAnimationFrame(render);
      return;
    }
    credit.style.opacity = "0";

    const time = (elapsed - introDuration) % cycleDuration;
    const growth = smoothstep(0, 4.6, time);
    const collapse = smoothstep(8.8, 10.8, time);
    const scatter = smoothstep(7.7, 9.0, time) * (1 - collapse);
    const opening = smoothstep(0.6, 5.1, time);
    const scale = (0.012 + growth * 0.988) * (1 - collapse);
    const opacity = smoothstep(0, 0.3, time) * (1 - smoothstep(9.6, 10.9, time));
    const turn = time * 0.18 + 0.15;
    const tilt = 0.62 + Math.sin(time * 0.38) * 0.11;
    const sinTurn = Math.sin(turn);
    const cosTurn = Math.cos(turn);
    const sinTilt = Math.sin(tilt);
    const cosTilt = Math.cos(tilt);
    const screenScale = Math.min(width * 0.34, height * 0.25);
    const projected = [];

    for (const point of particles) {
      const unfurl = 0.28 + opening * 0.72;
      let x = point.x * unfurl + point.scatterX * scatter;
      let y = point.y * unfurl + point.scatterY * scatter;
      let z = point.z + (1 - opening) * point.u * 0.9 + point.scatterZ * scatter;
      const rotatedX = x * cosTurn - y * sinTurn;
      const rotatedY = x * sinTurn + y * cosTurn;
      const screenY = rotatedY * cosTilt - z * sinTilt;
      const depth = rotatedY * sinTilt + z * cosTilt;
      const perspective = 4 / (4 - depth * scale * 0.35);
      const light = Math.max(0, Math.min(1,
        0.25 + point.shade * 0.30 + Math.abs(point.v) * 0.30
        + point.u * 0.15 + depth * 0.10));
      const disintegration = smoothstep(7.9 + point.shade * 0.6, 10.2, time);
      projected.push({
        x: width / 2 + rotatedX * screenScale * scale * perspective,
        y: height / 2 - screenY * screenScale * scale * perspective,
        depth,
        size: Math.max(0.35, point.size * screenScale / 165 * perspective),
        alpha: opacity * (0.65 + light * 0.35),
        red: Math.round(153 + light * 102 + disintegration * 15),
        green: Math.round(23 + light * 167 + disintegration * 45),
        blue: Math.round((76 + light * 135) * (1 - disintegration * 0.65))
      });
    }
    projected.sort((a, b) => a.depth - b.depth);
    for (const point of projected) {
      context.fillStyle = `rgba(${point.red},${point.green},${point.blue},${point.alpha})`;
      context.fillRect(point.x, point.y, point.size, point.size);
    }
    requestAnimationFrame(render);
  }

  // Keep the intro and bloom timeline stationary while the app is backgrounded.
  document.addEventListener("visibilitychange", () => {
    if (document.hidden) pauseTime = performance.now();
    else if (pauseTime !== undefined) {
      if (startTime !== undefined) startTime += performance.now() - pauseTime;
      pauseTime = undefined;
    }
  });
  window.addEventListener("resize", resize);
  resize();
  requestAnimationFrame(render);
})();
