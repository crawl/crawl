import $ from "jquery";

window.socket = null;

function send_message(msg, data) {
  data = data || {};
  data.msg = msg;
  socket.send(JSON.stringify(data));
}

// JSON message handlers
const message_handlers = {};
const immediate_handlers = {};

function register_message_handlers(dict) {
  $.extend(message_handlers, dict);
}

function register_immediate_handlers(dict) {
  $.extend(immediate_handlers, dict);
}

function handle_message(msg) {
  const handler = message_handlers[msg.msg];
  if (!handler) {
    console.error(`Unknown message type: ${msg.msg}`);
    return;
  }
  try {
    handler(msg);
  } catch (e) {
    console.error("Error handling message", msg);
    console.error(e);
    throw e;
  }
}

function handle_message_immediately(msg) {
  const handler = immediate_handlers[msg.msg];
  if (handler) {
    try {
      return handler(msg);
    } catch (e) {
      console.error("Error handling message immediately", msg);
      console.error(e);
      throw e;
    }
  } else return false;
}

export default {
  send_message,
  register_handlers: register_message_handlers,
  register_immediate_handlers,
  handle_message,
  handle_message_immediately,
};
