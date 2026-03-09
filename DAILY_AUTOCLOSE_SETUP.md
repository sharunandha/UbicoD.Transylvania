# Daily Auto-Close Report Setup (Sales, Top Items, Wastage, Stock Variance)

This guide adds an automatic end-of-day report emailed to the owner.

## 1) What to add in your current architecture

Your app already has:
- Orders in `restaurants/main/orders`
- Inventory in `restaurants/main` document (`inventory` field)

To generate a reliable daily close report, add these extra collections:

- `restaurants/main/stockMovements`
  - One document per stock change
  - Fields:
    - `itemName` (string)
    - `type` ("sale" | "restock" | "wastage" | "adjustment")
    - `qty` (number)
    - `timestamp` (ISO string or Firestore Timestamp)
    - `userEmail` (string)
    - `notes` (string, optional)

- `restaurants/main/closingCounts/{yyyy-mm-dd}`
  - Physical count entered by staff at close
  - Fields:
    - `counts` (map: itemName -> number)
    - `submittedBy` (string)
    - `submittedAt` (timestamp)

- `restaurants/main/dailyReports/{yyyy-mm-dd}`
  - Generated summary report

## 2) Report formulas

For each item and date D:

- `soldQty` = sum of order item quantities on D
- `restockedQty` = sum of stockMovements where type = restock on D
- `wastageQty` = sum of stockMovements where type = wastage on D
- `openingStock` = previous day closing count (or opening snapshot)
- `expectedClosing = openingStock + restockedQty - soldQty - wastageQty`
- `physicalClosing` = closingCounts[D].counts[item]
- `stockVariance = physicalClosing - expectedClosing`

Daily summary:
- `totalSales` = sum of all order totals on D
- `topItems` = items sorted by sold quantity desc
- `totalWastage` = sum of wastage quantities
- `varianceItems` = all items with non-zero variance

## 3) Add backend (required for secure email)

Do **not** send email from frontend JS. Use Firebase Cloud Functions + Secret Manager.

### 3.1 Create functions folder

```bash
firebase init functions
```

Choose:
- JavaScript or TypeScript (example below uses JavaScript)
- Node 20 runtime

### 3.2 Install packages

```bash
cd functions
npm install firebase-admin firebase-functions @sendgrid/mail
```

### 3.3 Set secrets

```bash
firebase functions:secrets:set SENDGRID_API_KEY
firebase functions:secrets:set OWNER_EMAIL
```

## 4) Scheduled function example

Create or update `functions/index.js`:

```javascript
const { onSchedule } = require("firebase-functions/v2/scheduler");
const { defineSecret } = require("firebase-functions/params");
const admin = require("firebase-admin");
const sgMail = require("@sendgrid/mail");

admin.initializeApp();
const db = admin.firestore();

const SENDGRID_API_KEY = defineSecret("SENDGRID_API_KEY");
const OWNER_EMAIL = defineSecret("OWNER_EMAIL");

function toDateRangeIST(date = new Date()) {
  // Build day range in Asia/Kolkata
  const fmt = new Intl.DateTimeFormat("en-CA", {
    timeZone: "Asia/Kolkata",
    year: "numeric",
    month: "2-digit",
    day: "2-digit"
  });
  const day = fmt.format(date); // yyyy-mm-dd
  const start = new Date(`${day}T00:00:00+05:30`);
  const end = new Date(`${day}T23:59:59.999+05:30`);
  return { day, start, end };
}

function asDate(value) {
  if (!value) return null;
  if (value.toDate) return value.toDate();
  if (typeof value === "string" || typeof value === "number") return new Date(value);
  if (value.seconds) return new Date(value.seconds * 1000);
  return null;
}

exports.dailyAutoCloseReport = onSchedule(
  {
    schedule: "5 23 * * *", // 11:05 PM daily
    timeZone: "Asia/Kolkata",
    secrets: [SENDGRID_API_KEY, OWNER_EMAIL],
    region: "asia-south1"
  },
  async () => {
    const restaurantRef = db.collection("restaurants").doc("main");
    const { day, start, end } = toDateRangeIST();

    // 1) Orders
    const ordersSnap = await restaurantRef.collection("orders")
      .where("timestamp", ">=", start.toISOString())
      .where("timestamp", "<=", end.toISOString())
      .get();

    let totalSales = 0;
    const itemSalesMap = new Map();

    ordersSnap.forEach((doc) => {
      const order = doc.data();
      totalSales += Number(order.total || 0);
      const items = Array.isArray(order.items) ? order.items : [];
      for (const item of items) {
        const name = item.name || "Unknown";
        const qty = Number(item.qty || item.quantity || 0);
        itemSalesMap.set(name, (itemSalesMap.get(name) || 0) + qty);
      }
    });

    // 2) Stock movements
    const movementSnap = await restaurantRef.collection("stockMovements")
      .where("timestamp", ">=", start.toISOString())
      .where("timestamp", "<=", end.toISOString())
      .get();

    const restockMap = new Map();
    const wastageMap = new Map();

    movementSnap.forEach((doc) => {
      const m = doc.data();
      const itemName = m.itemName;
      const qty = Number(m.qty || 0);
      if (!itemName) return;

      if (m.type === "restock") {
        restockMap.set(itemName, (restockMap.get(itemName) || 0) + qty);
      }
      if (m.type === "wastage") {
        wastageMap.set(itemName, (wastageMap.get(itemName) || 0) + qty);
      }
    });

    // 3) Opening stock from previous day report closing counts (fallback 0)
    const prev = new Date(start);
    prev.setDate(prev.getDate() - 1);
    const prevDay = new Intl.DateTimeFormat("en-CA", {
      timeZone: "Asia/Kolkata",
      year: "numeric",
      month: "2-digit",
      day: "2-digit"
    }).format(prev);

    const prevReportDoc = await restaurantRef.collection("dailyReports").doc(prevDay).get();
    const prevClosing = prevReportDoc.exists ? (prevReportDoc.data().closingCounts || {}) : {};

    // 4) Today physical closing counts
    const closingDoc = await restaurantRef.collection("closingCounts").doc(day).get();
    const closingCounts = closingDoc.exists ? (closingDoc.data().counts || {}) : {};

    // 5) Compute variance
    const allItems = new Set([
      ...Object.keys(prevClosing),
      ...Object.keys(closingCounts),
      ...Array.from(itemSalesMap.keys()),
      ...Array.from(restockMap.keys()),
      ...Array.from(wastageMap.keys())
    ]);

    const stockVariance = {};
    let totalWastage = 0;

    for (const item of allItems) {
      const opening = Number(prevClosing[item] || 0);
      const sold = Number(itemSalesMap.get(item) || 0);
      const restocked = Number(restockMap.get(item) || 0);
      const wastage = Number(wastageMap.get(item) || 0);
      const expected = opening + restocked - sold - wastage;
      const physical = Number(closingCounts[item] || 0);
      const variance = physical - expected;

      totalWastage += wastage;
      stockVariance[item] = {
        opening,
        sold,
        restocked,
        wastage,
        expectedClosing: expected,
        physicalClosing: physical,
        variance
      };
    }

    const topItems = [...itemSalesMap.entries()]
      .map(([name, qty]) => ({ name, qty }))
      .sort((a, b) => b.qty - a.qty)
      .slice(0, 10);

    const reportDoc = {
      day,
      generatedAt: admin.firestore.FieldValue.serverTimestamp(),
      totalSales,
      totalOrders: ordersSnap.size,
      topItems,
      totalWastage,
      stockVariance,
      closingCounts,
      emailSent: false
    };

    await restaurantRef.collection("dailyReports").doc(day).set(reportDoc, { merge: true });

    // 6) Send email
    sgMail.setApiKey(SENDGRID_API_KEY.value());

    const varianceRows = Object.entries(stockVariance)
      .filter(([, v]) => Number(v.variance) !== 0)
      .slice(0, 25)
      .map(([name, v]) =>
        `<tr><td>${name}</td><td>${v.opening}</td><td>${v.sold}</td><td>${v.restocked}</td><td>${v.wastage}</td><td>${v.expectedClosing}</td><td>${v.physicalClosing}</td><td>${v.variance}</td></tr>`
      )
      .join("");

    const topItemsRows = topItems
      .map((i) => `<li>${i.name}: ${i.qty}</li>`)
      .join("");

    const html = `
      <h2>Kitchen Alert - Daily Auto-Close (${day})</h2>
      <p><b>Total Sales:</b> ₹${totalSales.toFixed(2)}</p>
      <p><b>Total Orders:</b> ${ordersSnap.size}</p>
      <p><b>Total Wastage:</b> ${totalWastage}</p>

      <h3>Top Items</h3>
      <ul>${topItemsRows || "<li>No sales</li>"}</ul>

      <h3>Stock Variance (non-zero)</h3>
      <table border="1" cellpadding="6" cellspacing="0">
        <thead>
          <tr>
            <th>Item</th><th>Open</th><th>Sold</th><th>Restock</th><th>Wastage</th><th>Expected</th><th>Physical</th><th>Variance</th>
          </tr>
        </thead>
        <tbody>${varianceRows || "<tr><td colspan='8'>No variance</td></tr>"}</tbody>
      </table>
    `;

    await sgMail.send({
      to: OWNER_EMAIL.value(),
      from: OWNER_EMAIL.value(), // verify this sender in SendGrid
      subject: `Daily Auto-Close Report - ${day}`,
      html
    });

    await restaurantRef.collection("dailyReports").doc(day).set({ emailSent: true }, { merge: true });
  }
);
```

## 5) Deploy function

From project root:

```bash
firebase deploy --only functions:dailyAutoCloseReport
```

## 6) UI changes you should add in this website

1. In inventory dashboard:
   - Add button: **Mark Wastage**
   - Add fields: item, qty, reason
   - Save to `stockMovements` with `type: "wastage"`

2. In restock flow:
   - Add write to `stockMovements` with `type: "restock"`

3. In order confirm flow:
   - For each sold item, add `stockMovements` entry with `type: "sale"`
   - Keep inventory deduction as-is

4. Add end-of-day modal:
   - Staff enters physical closing counts item-wise
   - Save to `closingCounts/{yyyy-mm-dd}`

Without these 4 writes, variance and wastage accuracy will be weak.

## 7) Firestore indexing note

If Firestore asks for an index for timestamp queries, click the generated index link in Firebase console and create it.

## 8) Security minimum for production

- Restrict writes by authenticated staff only.
- Restrict report read/email trigger access to admin role.
- Keep SendGrid API key only in function secrets, never in frontend files.

## 9) Optional improvements

- Add a retry queue if email provider fails.
- Send report to multiple recipients.
- Add weekly summary scheduled every Monday.
- Export CSV attachment from the daily report.

---

If you want, next I can implement the first UI part in this project itself: add **Mark Wastage** + **stockMovements writes** into your current inventory/order code.
