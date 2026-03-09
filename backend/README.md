# Happy Ending Backend

Backend bridge for website -> ESP8266 kitchen display.

## Setup

1. Open terminal in `backend/`
2. Install dependencies:

```bash
npm install
```

3. Start server:

```bash
npm start
```

Server runs at: `http://localhost:8080`

## ESP8266 Setup

Update `esp8266_kitchen_display.ino`:

- Set `BACKEND_HOST` to your backend machine LAN IP, e.g.:

```cpp
const char* BACKEND_HOST = "http://192.168.1.50:8080";
```

Do NOT use `localhost` in ESP8266 firmware.

## API

### Website order submission
- `POST /api/orders`
- Body:

```json
{
  "id": "ORD-123",
  "table": "5",
  "items": [{ "name": "Chicken Briyani", "qty": 2 }],
  "total": 480,
  "time": "08:45 PM"
}
```

### ESP8266 fetch next order
- `GET /api/esp8266/next-order`
- Response 204 if no order.

### ESP8266 acknowledge displayed order
- `POST /api/esp8266/ack`

```json
{
  "orderId": "ORD-123",
  "status": "displayed"
}
```

### Health check
- `GET /api/health`
