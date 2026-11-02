document.addEventListener('DOMContentLoaded', () => {
    const nodeSelect = document.getElementById('nodeSelect');
    const chartTitle = document.getElementById('chartTitle');
    const statsNode = document.getElementById('statsNode');
    const errorEl = document.getElementById('chartError');
    const jsonOutput = document.getElementById('jsonOutput');
    let chart = null;
    let requestId = 0;

    async function loadNode(node) {
        const currentRequest = ++requestId;
        chartTitle.textContent = 'Sensor Node ' + node.replace(/^node_/i, '');
        statsNode.textContent = node;
        errorEl.textContent = '';
        try {
            const response = await fetch('/data.php?node=' + encodeURIComponent(node), {cache: 'no-store'});
            if (!response.ok) throw new Error('Server returned HTTP ' + response.status);
            const rows = await response.json();
            if (!Array.isArray(rows)) throw new Error('Expected a JSON array of readings');
            if (currentRequest !== requestId) return;
            jsonOutput.textContent = JSON.stringify(rows, null, 2);

            const times = rows.map(row => row.time_received);
            const temperatures = rows.map(row => Number(row.temperature)).filter(Number.isFinite);
            const humidities = rows.map(row => Number(row.humidity)).filter(Number.isFinite);
            const average = values => values.length ? (values.reduce((a,b) => a+b, 0)/values.length).toFixed(2) : '—';
            const max = values => values.length ? Math.max(...values).toFixed(2) : '—';
            const min = values => values.length ? Math.min(...values).toFixed(2) : '—';
            document.getElementById('avgTemp').textContent = average(temperatures) + (temperatures.length ? ' F' : '');
            document.getElementById('avgHum').textContent = average(humidities) + (humidities.length ? ' %' : '');
            document.getElementById('maxTemp').textContent = max(temperatures) + (temperatures.length ? ' F' : '');
            document.getElementById('minHum').textContent = min(humidities) + (humidities.length ? ' %' : '');

            if (chart) chart.destroy();
            chart = new Chart(document.getElementById('mycanvas'), {
                type: 'line',
                data: {
                    labels: times,
                    datasets: [{
                        label: 'Sensor Node ' + node.replace(/^node_/i, ''),
                        data: rows.map(row => Number(row.temperature)),
                        backgroundColor: 'rgba(0, 128, 0, 0.5)',
                        borderColor: 'rgba(0, 128, 0, 1)',
                        borderWidth: 1
                    }]
                },
                options: {
                    responsive: true,
                    scales: {
                        x: { title: {display: true, text: 'Time'} },
                        y: {beginAtZero: true, title: {display: true, text: 'Temperature (F)'} }
                    }
                }
            });
        } catch (error) {
            if (currentRequest !== requestId) return;
            console.error('Sensor chart failed:', error);
            errorEl.textContent = 'Unable to load sensor data: ' + error.message;
        }
    }

    if (nodeSelect && nodeSelect.options.length) {
        nodeSelect.addEventListener('change', () => loadNode(nodeSelect.value));
        loadNode(nodeSelect.value);
    } else {
        errorEl.textContent = 'No registered sensor nodes found.';
    }
});
