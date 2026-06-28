// ============================
// ESPChat v2 - script.js
// ============================

const ws = new WebSocket("ws://" + location.hostname + ":81/");

const loginScreen = document.getElementById("loginScreen");
const chatScreen = document.getElementById("chatScreen");

const loginName = document.getElementById("loginName");

const chat = document.getElementById("chat");

const messageInput = document.getElementById("message");

let username = "";

// ----------------------
// Auto Login
// ----------------------

window.onload = () => {
  const saved = localStorage.getItem("username");

  if (saved) {
    loginName.value = saved;

    joinChat();
  }
};

// ----------------------
// Join Chat
// ----------------------

function joinChat() {
  username = loginName.value.trim();

  if (username === "") return;

  localStorage.setItem("username", username);

  loginScreen.style.display = "none";

  chatScreen.style.display = "flex";

  messageInput.focus();
}

// ----------------------
// Send Message
// ----------------------

function send() {
  const text = messageInput.value.trim();

  if (text === "") return;

  const packet = {
    id: Date.now(),

    type: "message",

    username: username,

    message: text,

    time: new Date().toLocaleTimeString([], {
      hour: "2-digit",

      minute: "2-digit",
    }),
  };

  ws.send(JSON.stringify(packet));

  messageInput.value = "";
}

// ----------------------
// Receive Message
// ----------------------

ws.onmessage = (event) => {

    const data = JSON.parse(event.data);

    if (data.type === "users")
    {
        renderUsers(data.users);

        document.getElementById("onlineCount").textContent = data.online;

        return;
    }

    if (data.type === "system")
    {
        addSystemMessage(data.message);

        return;
    }

    if (data.type === "message")
    {
        addMessage(data);

        return;
    }

};



// ----------------------
// Add Message
// ----------------------

function addMessage(data) {
  const bubble = document.createElement("div");

  bubble.className = "msg";

  const color = getColor(data.username);

  bubble.innerHTML = `

<div class="avatar" style="background:${color}">

${data.username.charAt(0).toUpperCase()}

</div>

<div class="content">

<div class="header">

<span class="user">

${data.username}

</span>

<span class="time">

${data.time}

</span>

</div>

<div class="body">

${data.message}

</div>

</div>

`;

  chat.appendChild(bubble);

  chat.scrollTop = chat.scrollHeight;
}

function addSystemMessage(text)
{
    const div = document.createElement("div");

    div.className = "systemMessage";

    div.innerHTML = `⚙️ ${text}`;

    chat.appendChild(div);

    chat.scrollTop = chat.scrollHeight;
}

function renderUsers(list)
{
    const users = document.getElementById("users");

    if (!users) return;

    users.innerHTML = "";

    list.forEach(name => {

        const color = getColor(name);

        users.innerHTML += `
            <div class="userItem">

                <div class="avatar"
                    style="width:36px;height:36px;font-size:14px;background:${color};">

                    ${name.charAt(0).toUpperCase()}

                </div>

                <div>${name}</div>

            </div>
        `;

    });
}

// ----------------------
// Avatar Color
// ----------------------

function getColor(name) {
  const colors = [
    "#5865F2",
    "#57F287",
    "#FEE75C",
    "#EB459E",
    "#ED4245",
    "#3BA55D",
    "#FAA61A",
  ];

  let hash = 0;

  for (let i = 0; i < name.length; i++) hash += name.charCodeAt(i);

  return colors[hash % colors.length];
}

// ----------------------
// Enter to Send
// ----------------------

messageInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter") send();
});

// ----------------------
// Connected
// ----------------------

ws.onopen = () => {
  console.log("Connected");

  if (username !== "") {
    ws.send(
      JSON.stringify({
        type: "join",
        username: username,
      }),
    );
  }
};

// ----------------------
// Reconnect
// ----------------------

ws.onclose = () => {
  console.log("Disconnected");
};
