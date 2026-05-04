<template>
  <div ref="chartRef" class="chart"></div>
</template>

<script setup lang="ts">
import * as echarts from 'echarts'
import { onBeforeUnmount, onMounted, ref, watch } from 'vue'

const props = defineProps<{
  title: string
  labels: string[]
  series: { name: string; data: number[]; color: string }[]
}>()

const chartRef = ref<HTMLDivElement | null>(null)
let chart: echarts.ECharts | null = null

function renderChart() {
  if (!chartRef.value) return
  if (!chart) {
    chart = echarts.init(chartRef.value)
  }

  chart.setOption({
    backgroundColor: 'transparent',
    tooltip: { trigger: 'axis' },
    legend: { textStyle: { color: '#cbd5e1' } },
    grid: { left: 40, right: 20, top: 50, bottom: 30 },
    xAxis: {
      type: 'category',
      data: props.labels,
      axisLine: { lineStyle: { color: 'rgba(148,163,184,0.3)' } },
      axisLabel: { color: '#94a3b8' }
    },
    yAxis: {
      type: 'value',
      axisLine: { show: false },
      splitLine: { lineStyle: { color: 'rgba(148,163,184,0.12)' } },
      axisLabel: { color: '#94a3b8' }
    },
    series: props.series.map((item) => ({
      name: item.name,
      type: 'line',
      smooth: true,
      showSymbol: false,
      data: item.data,
      lineStyle: { width: 3, color: item.color },
      itemStyle: { color: item.color },
      areaStyle: { color: item.color + '22' }
    }))
  })
}

onMounted(() => {
  renderChart()
  window.addEventListener('resize', renderChart)
})

watch(() => [props.labels, props.series], renderChart, { deep: true })

onBeforeUnmount(() => {
  window.removeEventListener('resize', renderChart)
  chart?.dispose()
})
</script>

<style scoped>
.chart {
  width: 100%;
  height: 360px;
}
</style>
