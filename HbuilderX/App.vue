<script>
export default {
  onLaunch() {
    console.log("HomeDevice app launch");
  }
};
</script>

<style>
page {
  background: linear-gradient(160deg, #eef7f3 0%, #d9ebff 100%);
  color: #1b2a41;
  font-family: "PingFang SC", "Microsoft YaHei", sans-serif;
}
.card {
  background: rgba(255, 255, 255, 0.92);
  border-radius: 14px;
  padding: 20rpx;
  margin: 18rpx;
  box-shadow: 0 8rpx 20rpx rgba(26, 44, 75, 0.08);
}
.title {
  font-weight: 700;
  margin-bottom: 12rpx;
}
.row {
  display: flex;
  justify-content: space-between;
  padding: 8rpx 0;
}
.btn {
  margin: 8rpx;
}
</style>

