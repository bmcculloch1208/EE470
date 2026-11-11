
<?php
/*
 * EE 470 - IoT Sensor Dashboard
 * Displays registered nodes, measurements,
 * charts, statistics, and activity counts.
 */

$host = "database";
$dbname = "BrandonMcCulloch";
$username = "db_BrandonMcCulloch";
$password = getenv("DB_PASSWORD");

try {
    $pdo = new PDO(
        "mysql:host=$host;dbname=$dbname;charset=utf8mb4",
        $username,
        $password,
        [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]
    );

    $register = $pdo->query(
        "SELECT * FROM sensor_register
         ORDER BY node_name ASC"
    )->fetchAll(PDO::FETCH_ASSOC);

    $data = $pdo->query(
        "SELECT * FROM sensor_data
         ORDER BY node_name ASC, time_received ASC"
    )->fetchAll(PDO::FETCH_ASSOC);

    $nodes = array_values(array_unique(
        array_column($register, 'node_name')
    ));

    sort($nodes, SORT_NATURAL);

} catch (PDOException $e) {
    error_log($e->getMessage());
    http_response_code(500);
    exit('Database connection failed.');
}

// Display database records in an HTML table
function displayTable(array $records): void {
    if (!$records) {
        echo '<p>No records found.</p>';
        return;
    }

    echo '<table><tr>';

    foreach (array_keys($records[0]) as $column) {
        echo '<th>' .
            htmlspecialchars(
                (string)$column,
                ENT_QUOTES,
                'UTF-8'
            ) . '</th>';
    }

    echo '</tr>';

    foreach ($records as $row) {
        echo '<tr>';

        foreach ($row as $value) {
            echo '<td>' .
                htmlspecialchars(
                    (string)($value ?? ''),
                    ENT_QUOTES,
                    'UTF-8'
                ) . '</td>';
        }

        echo '</tr>';
    }

    echo '</table>';
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport"
          content="width=device-width, initial-scale=1">

    <title>SSU IoT Lab</title>

    <style>
        body {
            font-family: Arial, sans-serif;
            background: #f4f4f4;
            text-align: center;
            padding: 30px;
        }

        table {
            margin: 20px auto 10px;
            border-collapse: collapse;
            background: white;
        }

        th, td {
            border: 1px solid #ccc;
            padding: 12px;
            text-align: left;
        }

        th {
            background: #91ba35;
            color: white;
        }

        tr:nth-child(even) {
            background: #edf3df;
        }

        .table-wrap {
            max-width: 100%;
            overflow-x: auto;
        }

        .chart-wrap {
            max-width: 850px;
            margin: 20px auto;
        }

        .stats {
            display: grid;
            grid-template-columns: repeat(2, minmax(0, 1fr));
            max-width: 700px;
            margin: 20px auto;
            gap: 12px;
        }

        .stat {
            background: white;
            border: 1px solid #ddd;
            padding: 15px;
            border-radius: 6px;
        }

        .stat strong {
            display: block;
            margin-top: 8px;
            font-size: 1.3rem;
        }

        select {
            padding: 8px;
            margin-left: 8px;
        }

        #chartError {
            color: #a00;
        }

        @media (max-width: 600px) {
            .stats {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>

<body>

    <h1>EE 470 - IoT Sensor Dashboard</h1>

    <h2>Registered Sensor Nodes</h2>
    <div class="table-wrap">
        <?php displayTable($register); ?>
    </div>

    <h2>Data Received</h2>
    <div class="table-wrap">
        <?php displayTable($data); ?>
    </div>

    <h2>Sensor Measurements Graph</h2>

    <label for="nodeSelect">
        Select Sensor Node:
    </label>

    <select id="nodeSelect">
        <?php foreach ($nodes as $nodeName): ?>
            <option value="<?= htmlspecialchars(
                (string)$nodeName,
                ENT_QUOTES,
                'UTF-8'
            ) ?>">
                <?= htmlspecialchars(
                    (string)$nodeName,
                    ENT_QUOTES,
                    'UTF-8'
                ) ?>
            </option>
        <?php endforeach; ?>
    </select>

    <h2 id="chartTitle">Sensor Node</h2>

    <div class="chart-wrap">
        <canvas id="mycanvas"></canvas>
    </div>

    <p id="chartError" role="status"></p>

    <h2>
        Statistics for
        <span id="statsNode">Selected Node</span>
    </h2>

    <div class="stats">

        <div class="stat">
            Average Distance
            <strong id="avgDistance">—</strong>
        </div>

        <div class="stat">
            Average Light
            <strong id="avgLight">—</strong>
        </div>

        <div class="stat">
            Minimum Distance
            <strong id="minDistance">—</strong>
        </div>

        <div class="stat">
            Minimum Light
            <strong id="minLight">—</strong>
        </div>

        <div class="stat">
            Maximum Distance
            <strong id="maxDistance">—</strong>
        </div>

        <div class="stat">
            Maximum Light
            <strong id="maxLight">—</strong>
        </div>

        <div class="stat">
            Valid Messages Received
            <strong id="messageCount">—</strong>
        </div>

    </div>

    <h2>Selected Node Data (JSON)</h2>

    <pre id="jsonOutput"
         style="max-width:850px;
                margin:15px auto;
                text-align:left;
                overflow:auto;
                background:white;
                padding:16px;
                border:1px solid #ccc;">
Select a node to load data.</pre>

    <script src="/js/chart.umd.js"></script>
    <script src="/app.js?v=3"></script>

</body>
</html>
