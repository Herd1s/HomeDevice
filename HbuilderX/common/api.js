import { getBaseUrl, getDeviceId } from "./config.js";

function request(method, path, data) {
  return new Promise((resolve, reject) => {
    uni.request({
      url: `${getBaseUrl()}${path}`,
      method,
      data,
      timeout: 8000,
      success: (res) => {
        if (res.statusCode >= 200 && res.statusCode < 300) {
          resolve(res.data);
        } else {
          reject(new Error(`HTTP ${res.statusCode}`));
        }
      },
      fail: reject
    });
  });
}

export function fetchStatus() {
  const deviceId = encodeURIComponent(getDeviceId());
  return request("GET", `/api/app/status?device_id=${deviceId}`);
}

export function fetchHistory(limit = 50, offset = 0) {
  return request("GET", `/api/app/history?limit=${limit}&offset=${offset}`);
}

export function sendControl(commandType, payload) {
  return request("POST", "/api/app/control", {
    device_id: getDeviceId(),
    command_type: commandType,
    payload: String(payload)
  });
}

export function connectAlarmSocket(onMessage, onStateChange) {
  const wsUrl = `${getBaseUrl().replace(/^http/, "ws")}/ws/alarm`;
  const socketTask = uni.connectSocket({ url: wsUrl });
  const hasTaskApi =
    socketTask &&
    typeof socketTask.onOpen === "function" &&
    typeof socketTask.onMessage === "function";

  if (onStateChange) onStateChange("connecting");

  if (hasTaskApi) {
    socketTask.onOpen(() => {
      if (onStateChange) onStateChange("ok");
      if (typeof socketTask.send === "function") {
        socketTask.send({ data: "ping" });
      }
    });
    socketTask.onMessage((evt) => {
      if (!evt || !evt.data) return;
      try {
        const data = JSON.parse(evt.data);
        if (onMessage) onMessage(data);
      } catch (e) {}
    });
    socketTask.onError(() => {
      if (onStateChange) onStateChange("error");
    });
    socketTask.onClose(() => {
      if (onStateChange) onStateChange("close");
    });
    return socketTask;
  }

  const onOpenHandler = () => {
    if (onStateChange) onStateChange("ok");
    if (typeof uni.sendSocketMessage === "function") {
      uni.sendSocketMessage({ data: "ping" });
    }
  };
  const onMessageHandler = (evt) => {
    if (!evt || !evt.data) return;
    try {
      const data = JSON.parse(evt.data);
      if (onMessage) onMessage(data);
    } catch (e) {}
  };
  const onErrorHandler = () => {
    if (onStateChange) onStateChange("error");
  };
  const onCloseHandler = () => {
    if (onStateChange) onStateChange("close");
  };

  if (typeof uni.onSocketOpen === "function") uni.onSocketOpen(onOpenHandler);
  if (typeof uni.onSocketMessage === "function") uni.onSocketMessage(onMessageHandler);
  if (typeof uni.onSocketError === "function") uni.onSocketError(onErrorHandler);
  if (typeof uni.onSocketClose === "function") uni.onSocketClose(onCloseHandler);

  return {
    close() {
      if (typeof uni.offSocketOpen === "function") uni.offSocketOpen(onOpenHandler);
      if (typeof uni.offSocketMessage === "function") uni.offSocketMessage(onMessageHandler);
      if (typeof uni.offSocketError === "function") uni.offSocketError(onErrorHandler);
      if (typeof uni.offSocketClose === "function") uni.offSocketClose(onCloseHandler);
      if (typeof uni.closeSocket === "function") {
        uni.closeSocket({});
      }
    }
  };
}
