const express = require('express');
const cors = require('cors');
const fs = require('fs');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 8080;

app.use(cors());
app.use(express.json({ limit: '1mb' }));

const dataDir = path.join(__dirname, 'data');
const queueFile = path.join(dataDir, 'order-queue.json');

if (!fs.existsSync(dataDir)) {
  fs.mkdirSync(dataDir, { recursive: true });
}
if (!fs.existsSync(queueFile)) {
  fs.writeFileSync(queueFile, JSON.stringify({ orders: [] }, null, 2));
}

function readQueue() {
  try {
    const raw = fs.readFileSync(queueFile, 'utf8');
    const parsed = JSON.parse(raw);
    if (!Array.isArray(parsed.orders)) {
      return { orders: [] };
    }
    return parsed;
  } catch (error) {
    console.error('Queue read error:', error.message);
    return { orders: [] };
  }
}

function writeQueue(payload) {
  fs.writeFileSync(queueFile, JSON.stringify(payload, null, 2));
}

app.get('/api/health', (req, res) => {
  res.json({ ok: true, service: 'happy-ending-backend', timestamp: new Date().toISOString() });
});

app.post('/api/orders', (req, res) => {
  const body = req.body || {};
  const orderId = body.id || body.orderId || `ORD-${Date.now()}`;

  if (!Array.isArray(body.items) || body.items.length === 0) {
    return res.status(400).json({ ok: false, error: 'Invalid order items' });
  }

  const queue = readQueue();
  const orderRecord = {
    orderId,
    table: String(body.table || '1'),
    items: body.items,
    total: Number(body.total || 0),
    time: body.time || new Date().toLocaleTimeString(),
    status: 'pending',
    createdAt: new Date().toISOString(),
    updatedAt: new Date().toISOString()
  };

  queue.orders.push(orderRecord);
  writeQueue(queue);

  res.status(201).json({ ok: true, orderId, queued: true });
});

app.get('/api/orders/recent', (req, res) => {
  const queue = readQueue();
  const recent = [...queue.orders].sort((a, b) => Date.parse(b.createdAt) - Date.parse(a.createdAt)).slice(0, 50);
  res.json({ ok: true, count: recent.length, orders: recent });
});

// ESP8266 pulls next pending order from backend.
app.get('/api/esp8266/next-order', (req, res) => {
  const queue = readQueue();
  const nextOrder = queue.orders.find((order) => order.status === 'pending');

  if (!nextOrder) {
    return res.status(204).send();
  }

  nextOrder.status = 'dispatched';
  nextOrder.updatedAt = new Date().toISOString();
  writeQueue(queue);

  res.json({
    orderId: nextOrder.orderId,
    table: nextOrder.table,
    items: nextOrder.items,
    total: nextOrder.total,
    time: nextOrder.time
  });
});

// ESP8266 confirms order shown on LCD.
app.post('/api/esp8266/ack', (req, res) => {
  const orderId = req.body && req.body.orderId;
  const status = (req.body && req.body.status) || 'displayed';

  if (!orderId) {
    return res.status(400).json({ ok: false, error: 'orderId is required' });
  }

  const queue = readQueue();
  const order = queue.orders.find((item) => item.orderId === orderId);

  if (!order) {
    return res.status(404).json({ ok: false, error: 'Order not found' });
  }

  order.status = status;
  order.updatedAt = new Date().toISOString();
  writeQueue(queue);

  res.json({ ok: true, orderId, status });
});

app.listen(PORT, () => {
  console.log(`Ubico D. Transylvania backend running on http://localhost:${PORT}`);
});
