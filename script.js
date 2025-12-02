//----------------------------------------------------------------
// MQTT SAFE PUBLISH
//----------------------------------------------------------------
const MQTT_URL = "wss://test.mosquitto.org:8081";
let mqttClient;

function safePublish(topic, message) {
  if (!mqttClient || !mqttClient.connected) return;
  mqttClient.publish(topic, message);
}

//----------------------------------------------------------------
// UI + MQTT CONNECTION
//----------------------------------------------------------------
function updateMQTTStatus(connected) {
  const light = document.getElementById("mqttLight");
  const text = document.getElementById("mqttStatus");

  if (connected) {
    light.style.background = "#4cc24f";
    text.innerText = "Connected";
  } else {
    light.style.background = "#c33";
    text.innerText = "Disconnected";
  }
}

document.getElementById("connectMQTT").onclick = () => {
  mqttClient = mqtt.connect(MQTT_URL);

  mqttClient.on("connect", () => {
    updateMQTTStatus(true);
    mqttClient.subscribe("esp32/status");
  });

  mqttClient.on("message", (topic, msg) => {
    if (topic === "esp32/status") {
      const [state, ip] = msg.toString().split("|");
      if (state === "ONLINE") {
        document.getElementById("deviceIP").innerText = ip;
      }
    }
  });
};

document.getElementById("pingDevice").onclick = () => {
  safePublish("esp32/ping", "PING");
};

//----------------------------------------------------------------
// ML5 BODYPOSE SETUP
//----------------------------------------------------------------
let video;
let bodyPose;
let poses = [];
let connections;

let lastPosture = "";
let stableFrames = 0;

function preload() {
  bodyPose = ml5.bodyPose();
}

function setup() {
  let canvas = createCanvas(640, 480);
  canvas.parent("videoContainer");

  video = createCapture(VIDEO);
  video.size(640, 480);
  video.hide();

  bodyPose.detectStart(video, gotPoses);
  connections = bodyPose.getSkeleton();
}

//----------------------------------------------------------------
// POSTURE ALGORITHM
//----------------------------------------------------------------
function computePosture(pose) {
  const nose = pose.keypoints[0];
  const L = pose.keypoints[5];
  const R = pose.keypoints[6];

  if (nose.confidence < 0.2) return;

  const box = document.getElementById("postureBox");

  if (box.innerText.includes("Waiting")) {
    box.innerText = "Detecting posture…";
  }

  let shoulderY = (L.y + R.y) / 2;
  let delta = nose.y - shoulderY;

  document.getElementById("noseVal").innerText = nose.y.toFixed(1);
  document.getElementById("shoulderVal").innerText = shoulderY.toFixed(1);
  document.getElementById("deltaVal").innerText = delta.toFixed(1);

  const posture = delta > -100 ? "S" : "U";

  if (posture === "U") {
    box.innerText = "👍 Upright";
    box.className = "good";
  } else {
    box.innerText = "⚠️ Slouching";
    box.className = "bad";
  }

  if (posture !== lastPosture) {
    stableFrames++;

    if (stableFrames > 5) {
      lastPosture = posture;

      safePublish("esp32/posture", posture);

      if (posture === "U") safePublish("esp32/mode", "MODE_5");
      else safePublish("esp32/mode", "MODE_1");

      stableFrames = 0;
    }
  } else {
    stableFrames = 0;
  }
}

//----------------------------------------------------------------
// DRAW LOOP
//----------------------------------------------------------------
function draw() {
  image(video, 0, 0, width, height);

  for (let pose of poses) {
    computePosture(pose);

    // Draw red skeleton lines
    for (let c of connections) {
      let a = pose.keypoints[c[0]];
      let b = pose.keypoints[c[1]];
      if (a.confidence > 0.2 && b.confidence > 0.2) {
        stroke(255, 0, 0); // red lines
        strokeWeight(2);
        line(a.x, a.y, b.x, b.y);
      }
    }

    // Draw neon green keypoints
    for (let kp of pose.keypoints) {
      if (kp.confidence > 0.2) {
        fill(0, 255, 0); // bright green
        noStroke();
        circle(kp.x, kp.y, 10); // large, clear
      }
    }
  }
}

function gotPoses(results) {
  poses = results;
}
