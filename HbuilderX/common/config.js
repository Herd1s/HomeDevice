// On phones, 127.0.0.1 is phone local loopback.
const DEFAULT_BASE_URL = "http://42.192.113.88";
const DEFAULT_DEVICE_ID = "HD-001";

export function getBaseUrl() {
  return uni.getStorageSync("baseUrl") || DEFAULT_BASE_URL;
}

export function setBaseUrl(url) {
  uni.setStorageSync("baseUrl", url);
}

export function getDeviceId() {
  return uni.getStorageSync("deviceId") || DEFAULT_DEVICE_ID;
}

export function setDeviceId(deviceId) {
  uni.setStorageSync("deviceId", deviceId);
}

