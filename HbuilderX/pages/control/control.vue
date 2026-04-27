<template>
  <view>
    <view class="card">
      <view class="title">布撤防</view>
      <button class="btn" @click="send('SET_MODE','DISARM')">撤防</button>
      <button class="btn" @click="send('SET_MODE','ARM')">布防</button>
      <button class="btn" @click="send('SET_MODE','AUTO')">自动</button>
    </view>

    <view class="card">
      <view class="title">执行控制</view>
      <button class="btn" type="primary" @click="send('SET_RELAY','1')">启动喷淋</button>
      <button class="btn" @click="send('SET_RELAY','0')">关闭喷淋</button>
      <button class="btn" @click="send('SILENCE','1')">蜂鸣器消音</button>
    </view>

    <view class="card">
      <view class="title">烟雾阈值</view>
      <input type="number" v-model="smokeTh" placeholder="600" />
      <button class="btn" @click="setTh">下发阈值</button>
    </view>
  </view>
</template>

<script>
import { sendControl } from "../../common/api.js";

export default {
  data() {
    return {
      smokeTh: "600"
    };
  },
  methods: {
    async send(cmd, payload) {
      try {
        const res = await sendControl(cmd, payload);
        if (res && res.ok) {
          uni.showToast({ title: `命令#${res.command_id}`, icon: "none" });
        } else {
          uni.showToast({ title: "下发失败", icon: "none" });
        }
      } catch (e) {
        uni.showToast({ title: "网络错误", icon: "none" });
      }
    },
    async setTh() {
      const v = parseInt(this.smokeTh, 10);
      if (Number.isNaN(v) || v < 1 || v > 4095) {
        uni.showToast({ title: "阈值范围1~4095", icon: "none" });
        return;
      }
      await this.send("SET_SMOKE_TH", String(v));
    }
  }
};
</script>

