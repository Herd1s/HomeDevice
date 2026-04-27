<template>
  <view class="page-container">
    <view class="header">
      <view class="title-box">
        <text class="page-title">设备控制台</text>
        <text class="update-time">上次更新: {{ status.updated_at || "暂无数据" }}</text>
      </view>
      <view class="device-badge">ID: {{ status.device_id || "----" }}</view>
    </view>

    <view class="section-title">环境数据</view>
    <view class="data-grid">
      <view class="grid-card">
        <view class="icon-wrap bg-red">🌡️</view>
        <view class="value-box">
          <text class="value">{{ status.temperature }}</text>
          <text class="unit">℃</text>
        </view>
        <text class="label">温度</text>
      </view>

      <view class="grid-card">
        <view class="icon-wrap bg-blue">💧</view>
        <view class="value-box">
          <text class="value">{{ status.humidity }}</text>
          <text class="unit">%</text>
        </view>
        <text class="label">湿度</text>
      </view>

      <view class="grid-card">
        <view class="icon-wrap bg-purple">💨</view>
        <view class="value-box">
          <text class="value">{{ status.smoke }}</text>
        </view>
        <text class="label">烟雾浓度</text>
      </view>
    </view>

    <view class="section-title">设备状态</view>
    <view class="list-card">
      <view class="list-item">
        <view class="item-left">
          <text class="icon-text">🔥</text>
          <text>火焰检测</text>
        </view>
        <text :class="['badge', status.flame ? 'badge-danger' : 'badge-safe']">
          {{ yesNo(status.flame) }}
        </text>
      </view>

      <view class="list-item">
        <view class="item-left">
          <text class="icon-text">🚶‍♂️</text>
          <text>入侵检测</text>
        </view>
        <text :class="['badge', status.intrusion ? 'badge-danger' : 'badge-safe']">
          {{ yesNo(status.intrusion) }}
        </text>
      </view>

      <view class="list-item">
        <view class="item-left">
          <text class="icon-text">🛡️</text>
          <text>安防模式</text>
        </view>
        <text class="badge badge-info">{{ status.mode }}</text>
      </view>

      <view class="list-item">
        <view class="item-left">
          <text class="icon-text">🔌</text>
          <text>继电器</text>
        </view>
        <text :class="['badge', status.relay ? 'badge-active' : 'badge-off']">
          {{ yesNo(status.relay) }}
        </text>
      </view>

      <view class="list-item no-border">
        <view class="item-left">
          <text class="icon-text">🔔</text>
          <text>蜂鸣器报警</text>
        </view>
        <text :class="['badge', status.buzzer ? 'badge-danger' : 'badge-off']">
          {{ yesNo(status.buzzer) }}
        </text>
      </view>
    </view>

    <view class="section-title">系统连接</view>
    <view class="list-card">
      <view class="list-item">
        <view class="item-left">
          <view :class="['dot', isHttpHealthy() ? 'dot-green' : 'dot-red']"></view>
          <text>HTTP 状态</text>
        </view>
        <text class="status-text">{{ httpState }}</text>
      </view>
      
      <view class="list-item">
        <view class="item-left">
          <view :class="['dot', isWsHealthy() ? 'dot-green' : 'dot-red']"></view>
          <text>WebSocket</text>
        </view>
        <text class="status-text">{{ wsState }}</text>
      </view>

      <view class="list-item alarm-item no-border" v-if="lastAlarm">
        <view class="item-left">
          <text class="icon-text">⚠️</text>
          <text class="text-danger">最近告警</text>
        </view>
        <text class="alarm-msg">{{ lastAlarm }}</text>
      </view>
    </view>

    <button class="btn-refresh" hover-class="btn-hover" @click="loadStatus">立即刷新</button>
  </view>
</template>

<script>
import { connectAlarmSocket, fetchStatus } from "../../common/api.js";

export default {
  data() {
    return {
      status: {
        temperature: 0,
        humidity: 0,
        smoke: 0,
        flame: 0,
        intrusion: 0,
        mode: "DISARM",
        relay: 0,
        buzzer: 0
      },
      httpState: "idle",
      wsState: "idle",
      lastAlarm: "",
      timer: null,
      socketTask: null
    };
  },
  onShow() {
    this.loadStatus();
    this.timer = setInterval(() => this.loadStatus(), 2000);
    this.openWs();
  },
  onHide() {
    this.dispose();
  },
  onUnload() {
    this.dispose();
  },
  methods: {
    isHttpHealthy() {
      return this.httpState === "ok";
    },
    isWsHealthy() {
      return this.wsState === "connected" || this.wsState === "ok" || this.wsState === "open";
    },
    yesNo(v) {
      return Number(v) ? "是" : "否";
    },
    async loadStatus() {
      if (this.httpState === "idle") {
        this.httpState = "loading";
      }
      try {
        const res = await fetchStatus();
        if (res && res.ok && res.data) {
          this.status = res.data;
          this.httpState = "ok";
        } else {
          this.httpState = "no-data";
        }
      } catch (e) {
        this.httpState = "error";
      }
    },
    openWs() {
      if (this.socketTask) return;
      this.socketTask = connectAlarmSocket(
        (msg) => {
          if (msg.event === "status" && msg.data) this.status = msg.data;
          if (msg.event === "alarm" && msg.data) {
            this.lastAlarm = `${msg.data.alarm_type}: ${msg.data.message}`;
          }
        },
        (state) => {
          this.wsState = state;
        }
      );
    },
    dispose() {
      if (this.timer) {
        clearInterval(this.timer);
        this.timer = null;
      }
      if (this.socketTask) {
        this.socketTask.close({});
        this.socketTask = null;
      }
    }
  }
};
</script>

<style scoped>
.page-container {
  min-height: 100vh;
  background-color: #f4f5f9;
  padding: 20px 16px;
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  box-sizing: border-box;
}

/* 头部样式 */
.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 24px;
}
.title-box {
  display: flex;
  flex-direction: column;
}
.page-title {
  font-size: 24px;
  font-weight: bold;
  color: #333;
}
.update-time {
  font-size: 12px;
  color: #888;
  margin-top: 4px;
}
.device-badge {
  background: #e1e8ff;
  color: #4a6ee0;
  padding: 6px 12px;
  border-radius: 20px;
  font-size: 13px;
  font-weight: 500;
}

/* 标题样式 */
.section-title {
  font-size: 15px;
  font-weight: 600;
  color: #666;
  margin: 16px 0 10px 4px;
}

/* 数据网格卡片 */
.data-grid {
  display: flex;
  justify-content: space-between;
  margin-bottom: 16px;
}
.grid-card {
  width: 30%;
  background: #fff;
  border-radius: 16px;
  padding: 16px 10px;
  display: flex;
  flex-direction: column;
  align-items: center;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.03);
}
.icon-wrap {
  width: 40px;
  height: 40px;
  border-radius: 12px;
  display: flex;
  justify-content: center;
  align-items: center;
  font-size: 20px;
  margin-bottom: 10px;
}
.bg-red { background: #ffe3e3; }
.bg-blue { background: #e3f2fd; }
.bg-purple { background: #f3e5f5; }

.value-box {
  display: flex;
  align-items: baseline;
  margin-bottom: 4px;
}
.value {
  font-size: 22px;
  font-weight: bold;
  color: #333;
}
.unit {
  font-size: 12px;
  color: #888;
  margin-left: 2px;
}
.label {
  font-size: 13px;
  color: #666;
}

/* 列表卡片 */
.list-card {
  background: #fff;
  border-radius: 16px;
  padding: 0 16px;
  margin-bottom: 16px;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.03);
}
.list-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px 0;
  border-bottom: 1px solid #f0f0f0;
}
.no-border {
  border-bottom: none;
}
.item-left {
  display: flex;
  align-items: center;
  font-size: 15px;
  color: #333;
}
.icon-text {
  font-size: 18px;
  margin-right: 10px;
}

/* 标签样式 (Badge) */
.badge {
  padding: 4px 12px;
  border-radius: 6px;
  font-size: 13px;
  font-weight: 500;
}
.badge-safe { background: #e6f4ea; color: #1e8e3e; }
.badge-danger { background: #fce8e6; color: #d93025; }
.badge-info { background: #e8f0fe; color: #1a73e8; }
.badge-active { background: #fff4e5; color: #f68b1e; }
.badge-off { background: #f1f3f4; color: #5f6368; }

/* 系统状态小红点/绿点 */
.dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  margin-right: 10px;
}
.dot-green { background-color: #34c759; box-shadow: 0 0 6px rgba(52, 199, 89, 0.5); }
.dot-red { background-color: #ff3b30; box-shadow: 0 0 6px rgba(255, 59, 48, 0.5); }
.status-text {
  font-size: 14px;
  color: #888;
}

/* 告警信息 */
.alarm-item {
  flex-direction: column;
  align-items: flex-start;
}
.text-danger { color: #d93025; font-weight: bold;}
.alarm-msg {
  margin-top: 8px;
  font-size: 13px;
  color: #666;
  background: #fdf5f4;
  padding: 8px 12px;
  border-radius: 8px;
  width: 100%;
  box-sizing: border-box;
}

/* 刷新按钮 */
.btn-refresh {
  margin-top: 24px;
  background-color: #4a6ee0;
  color: #fff;
  border-radius: 24px;
  font-size: 16px;
  font-weight: 500;
  height: 48px;
  line-height: 48px;
  box-shadow: 0 4px 12px rgba(74, 110, 224, 0.3);
}
.btn-refresh::after {
  border: none;
}
.btn-hover {
  opacity: 0.8;
  transform: scale(0.98);
}
</style>
