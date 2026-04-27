<template>
  <view>
    <view class="card">
      <view class="title">系统设置</view>
      <view class="row"><text>服务器地址</text></view>
      <input v-model="baseUrl" placeholder="http://114.67.122.208:8000" />
      <view class="row"><text>设备ID</text></view>
      <input v-model="deviceId" placeholder="HD-001" />
      <button class="btn" type="primary" @click="save">保存</button>
      <button class="btn" @click="test">测试连接</button>
    </view>
  </view>
</template>

<script>
import { fetchStatus } from "../../common/api.js";
import { getBaseUrl, getDeviceId, setBaseUrl, setDeviceId } from "../../common/config.js";

export default {
  data() {
    return {
      baseUrl: "",
      deviceId: ""
    };
  },
  onShow() {
    this.baseUrl = getBaseUrl();
    this.deviceId = getDeviceId();
  },
  methods: {
    save() {
      setBaseUrl((this.baseUrl || "").trim());
      setDeviceId((this.deviceId || "HD-001").trim());
      uni.showToast({ title: "已保存", icon: "success" });
    },
    async test() {
      try {
        const res = await fetchStatus();
        if (res && res.ok) {
          uni.showToast({ title: "连接成功", icon: "success" });
        } else {
          uni.showToast({ title: "无设备数据", icon: "none" });
        }
      } catch (e) {
        uni.showToast({ title: "连接失败", icon: "none" });
      }
    }
  }
};
</script>

