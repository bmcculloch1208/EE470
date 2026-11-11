
document.addEventListener('DOMContentLoaded', () => {

    const nodeSelect = document.getElementById('nodeSelect');
    const chartTitle = document.getElementById('chartTitle');
    const statsNode = document.getElementById('statsNode');
    const errorEl = document.getElementById('chartError');
    const jsonOutput = document.getElementById('jsonOutput');

    let chart = null;
    let requestId = 0;

    // Calculate statistics
    const average = values => values.length
        ? (values.reduce((a, b) => a + b, 0) / values.length).toFixed(2)
        : '—';

    const maximum = values => values.length
        ? Math.max(...values).toFixed(2)
        : '—';

    const minimum = values => values.length
        ? Math.min(...values).toFixed(2)
        : '—';

    function formatValue(value, unit) {
        return value === '—' ? value : value + ' ' + unit;
    }

    // Load measurements for selected node
    async function loadNode(node) {

        const currentRequest = ++requestId;

        chartTitle.textContent =
            'Sensor Node ' + node.replace(/^node_/i, '');

        statsNode.textContent = node;
        errorEl.textContent = '';

        try {
            const response = await fetch(
                '/data.php?node=' + encodeURIComponent(node),
                { cache: 'no-store' }
            );

            if (!response.ok) {
                throw new Error(
                    'Server returned HTTP ' + response.status
                );
            }

            const result = await response.json();

            if (!Array.isArray(result.readings)) {
                throw new Error('Invalid sensor data format');
            }

            if (currentRequest !== requestId) return;

            const rows = result.readings;

            // Display JSON response
            jsonOutput.textContent =
                JSON.stringify(result, null, 2);

            // Extract measurements
            const times = rows.map(row => row.time_received);

            const distances = rows.map(row =>
                Number(row.distance)
            );

            const lights = rows.map(row =>
                Number(row.light)
            );

            // Exclude invalid values from statistics
            const validDistances =
                distances.filter(Number.isFinite);

            const validLights =
                lights.filter(Number.isFinite);

            // Update distance statistics
            document.getElementById('avgDistance').textContent =
                formatValue(average(validDistances), 'cm');

            document.getElementById('minDistance').textContent =
                formatValue(minimum(validDistances), 'cm');

            document.getElementById('maxDistance').textContent =
                formatValue(maximum(validDistances), 'cm');

            // Update light statistics
            document.getElementById('avgLight').textContent =
                formatValue(average(validLights), 'ADC');

            document.getElementById('minLight').textContent =
                formatValue(minimum(validLights), 'ADC');

            document.getElementById('maxLight').textContent =
                formatValue(maximum(validLights), 'ADC');

            // Update activity counter
            document.getElementById('messageCount').textContent =
                result.message_count ?? 0;

            // Destroy previous chart
            if (chart) {
                chart.destroy();
            }

            // Create new chart
            chart = new Chart(
                document.getElementById('mycanvas'),
                {
                    type: 'line',

                    data: {
                        labels: times,

                        datasets: [
                            {
                                label: 'Distance (cm)',
                                data: distances,
                                borderColor: 'rgb(0, 128, 0)',
                                backgroundColor:
                                    'rgba(0, 128, 0, 0.15)',
                                borderWidth: 2,
                                pointRadius: 3,
                                tension: 0.2,
                                yAxisID: 'yDistance'
                            },
                            {
                                label: 'Light (ADC)',
                                data: lights,
                                borderColor: 'rgb(37, 99, 235)',
                                backgroundColor:
                                    'rgba(37, 99, 235, 0.15)',
                                borderWidth: 2,
                                pointRadius: 3,
                                tension: 0.2,
                                yAxisID: 'yLight'
                            }
                        ]
                    },

                    options: {
                        responsive: true,

                        interaction: {
                            mode: 'index',
                            intersect: false
                        },

                        plugins: {
                            legend: {
                                display: true,
                                position: 'top'
                            }
                        },

                        scales: {
                            x: {
                                title: {
                                    display: true,
                                    text: 'Measurement Time'
                                }
                            },

                            yDistance: {
                                type: 'linear',
                                position: 'left',
                                beginAtZero: true,
                                title: {
                                    display: true,
                                    text: 'Distance (cm)'
                                }
                            },

                            yLight: {
                                type: 'linear',
                                position: 'right',
                                min: 0,
                                max: 1023,
                                title: {
                                    display: true,
                                    text: 'Light (ADC)'
                                },
                                grid: {
                                    drawOnChartArea: false
                                }
                            }
                        }
                    }
                }
            );

        } catch (error) {
            if (currentRequest !== requestId) return;

            console.error('Sensor chart failed:', error);

            errorEl.textContent =
                'Unable to load sensor data: ' + error.message;
        }
    }

    // Node selection
    if (nodeSelect && nodeSelect.options.length) {

        nodeSelect.addEventListener('change', () => {
            loadNode(nodeSelect.value);
        });

        loadNode(nodeSelect.value);

        // Refresh measurements every 10 seconds
        setInterval(() => {
            if (!document.hidden) {
                loadNode(nodeSelect.value);
            }
        }, 10000);

    } else {
        errorEl.textContent =
            'No registered sensor nodes found.';
    }
});
