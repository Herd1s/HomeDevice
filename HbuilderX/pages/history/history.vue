<template>
  <view>
    <view class="card">
      <view class="title">告警记录</view>
      <button class="btn" type="primary" @click="load">刷新</button>
    </view>
    <view v-if="rows.length === 0" class="card">
      <view class="row"><text>暂无记录</text></view>
    </view>
    <view v-for="item in rows" :key="item.id" class="card">
      <view class="row"><text>类型</text><text>{{ item.alarm_type }}</text></view>
      <view class="row"><text>等级</text><text>{{ item.level }}</text></view>
      <view class="row"><text>内容</text><text>{{ item.message }}</text></view>
      <view class="row"><text>时间</text><text>{{ item.created_at }}</text></view>
    </view>
  </view>
</template>

<script>
import { fetchHistory } from "../../common/api.js";

export default {
  data() {
    return {
      rows: []
    };
  },
  onShow() {
    this.load();
  },
  methods: {
    async load() {
      try {
        const res = await fetchHistory(50, 0);
        this.rows = (res && res.ok && res.data) ? res.data : [];
      } catch (e) {
        this.rows = [];
        uni.showToast({ title: "获取失败", icon: "none" });
      }
    }
  }
};
</script>

