// ============================
// ESPChat v2 - script.js
// ============================

let ws = null;

const loginScreen = document.getElementById("loginScreen");
const chatScreen = document.getElementById("chatScreen");

const loginName = document.getElementById("loginName");

const chat = document.getElementById("chat");

const messageInput = document.getElementById("message");

const emojiToggle = document.getElementById("emojiToggle");
const emojiPicker = document.getElementById("emojiPicker");

let username = "";

const typingIndicator =
document.getElementById("typingIndicator");

let typing = false;
let typingTimeout;

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

    console.log("JOIN CALLED");
    username = loginName.value.trim();

    if (username === "") return;

    localStorage.setItem("username", username);
    loginScreen.style.display = "none";
    chatScreen.style.display = "flex";
    messageInput.focus();

    if (ws && (ws.readyState === WebSocket.OPEN ||
               ws.readyState === WebSocket.CONNECTING)) {
        return;
    }

    ws = new WebSocket("ws://" + location.hostname + ":81/");

    ws.onopen = () => {
        console.log("Connected");

        ws.send(JSON.stringify({
            type: "join",
            username: username
        }));

        setConnectionStatus(true);
    };

    ws.onmessage = handleSocketMessage;

    ws.onclose = () => {
        console.log("Disconnected");
        setConnectionStatus(false);
        typingIndicator.textContent = "";
    };

    ws.onerror = () => {
        setConnectionStatus(false);
    };

}

function setConnectionStatus(connected) {
    const status = document.getElementById("connectionStatus");

    if (!status) return;

    status.firstChild.textContent =
        connected ? "🟢 Connected • " : "🔴 Disconnected • ";
}

// ----------------------
// Send Message
// ----------------------

function send() {
  const text = messageInput.value.trim();

  if (text === "" || !ws || ws.readyState !== WebSocket.OPEN) return;

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

  ws.send(JSON.stringify({
      type:"typing",
      username:username,
      typing:false
  }));

  typing = false;
  clearTimeout(typingTimeout);


}

// ----------------------
// Receive Message
// ----------------------

function handleSocketMessage(event) {

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

    if (data.type === "typing")
    {
        if (data.username === username)
            return;

        if (data.typing)
            typingIndicator.textContent = `${data.username} is typing...`;
        else
            typingIndicator.textContent = "";

        return;
    }

    if (data.type === "message")
    {
        addMessage(data);

        return;
    }

}



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
  if (e.key === "Enter") {
    send();
    closeEmojiPicker();
  }
});

emojiToggle.addEventListener("click", (event) => {
    event.stopPropagation();

    const opening = emojiPicker.hidden;
    emojiPicker.hidden = !opening;
    emojiToggle.setAttribute("aria-expanded", String(opening));
});

emojiPicker.addEventListener("click", (event) => {
    const button = event.target.closest("button[data-emoji]");

    if (!button) return;

    const emoji = button.dataset.emoji;
    const start = messageInput.selectionStart ?? messageInput.value.length;
    const end = messageInput.selectionEnd ?? messageInput.value.length;

    messageInput.value =
        messageInput.value.slice(0, start) +
        emoji +
        messageInput.value.slice(end);

    const cursor = start + emoji.length;
    messageInput.focus();
    messageInput.setSelectionRange(cursor, cursor);
});

document.addEventListener("click", (event) => {
    if (!emojiPicker.contains(event.target) && event.target !== emojiToggle) {
        closeEmojiPicker();
    }
});

document.addEventListener("keydown", (event) => {
    if (event.key === "Escape") closeEmojiPicker();
});

function closeEmojiPicker() {
    emojiPicker.hidden = true;
    emojiToggle.setAttribute("aria-expanded", "false");
}

messageInput.addEventListener("input",()=>{

    if (!ws || ws.readyState !== WebSocket.OPEN) return;

    if(!typing){

        ws.send(JSON.stringify({

            type:"typing",
            username:username,
            typing:true

        }));

        typing = true;
    }

    clearTimeout(typingTimeout);

    typingTimeout = setTimeout(()=>{

        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({
                type:"typing",
                username:username,
                typing:false
            }));
        }

        typing = false;

    },1000);

});
