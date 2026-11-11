
<?php
/*
 * EE 470 - Sensor Data API
 * Returns sensor readings and activity count as JSON.
 */

header('Content-Type: application/json; charset=UTF-8');
header('Cache-Control: no-store');

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

    // Selected node
    $node = $_GET['node'] ?? 'node_1';

    if (!is_string($node) ||
        !preg_match('/^[a-zA-Z0-9_-]{1,10}$/D', $node)) {
        http_response_code(400);
        echo json_encode(["error" => "Invalid node"]);
        exit;
    }

    // Retrieve distance and light measurements
    $stmt = $pdo->prepare(
        "SELECT distance, light, time_received
         FROM sensor_data
         WHERE node_name = ?
         ORDER BY time_received ASC"
    );

    $stmt->execute([$node]);
    $readings = $stmt->fetchAll(PDO::FETCH_ASSOC);

    // Retrieve the valid message counter
    $stmt = $pdo->prepare(
        "SELECT message_count
         FROM sensor_activity
         WHERE node_name = ?"
    );

    $stmt->execute([$node]);
    $count = $stmt->fetchColumn();

    echo json_encode([
        "node" => $node,
        "message_count" => $count === false ? 0 : (int)$count,
        "readings" => $readings
    ], JSON_THROW_ON_ERROR);

} catch (Throwable $e) {
    error_log($e->getMessage());

    http_response_code(500);
    echo json_encode([
        "error" => "Database query failed"
    ]);
}
