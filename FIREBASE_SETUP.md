# Firebase Setup Guide for Kitchen Alert System

## Step 1: Create Firebase Project

1. Go to https://console.firebase.google.com/
2. Click "Create a new project" or select existing project
3. Name: "Kitchen Alert" (or your preference)
4. Accept terms and create

## Step 2: Enable Firestore Database

1. In Firebase Console, go to **Build** → **Firestore Database**
2. Click **Create database**
3. Start in **Production mode**
4. Choose location: Select closest to your restaurant location
5. Click **Create**

## Step 3: Set Firestore Security Rules

1. Go to **Firestore Database** → **Rules** tab
2. Replace default rules with:

```
rules_version = '2';
service cloud.firestore {
    match /databases/{database}/documents {
        function signedIn() {
            return request.auth != null;
        }

        function isStaff() {
            return signedIn() && request.auth.token.email in [
                'sharunandha21@gmail.com',
                'admin321@restaurant.com'
            ];
        }

        match /restaurants/main {
            allow read: if signedIn();
            allow write: if isStaff();
        }

        match /restaurants/main/orders/{orderId} {
            allow read: if signedIn();
            allow create, update: if signedIn();
            allow delete: if isStaff();
        }

        match /restaurants/main/inventory/{docId} {
            allow read: if signedIn();
            allow write: if isStaff();
        }

        match /restaurants/main/menu/{docId} {
            allow read: if signedIn();
            allow write: if isStaff();
        }

        match /restaurants/main/analytics/{docId} {
            allow read: if signedIn();
            allow write: if isStaff();
        }

        match /restaurants/main/dailyAnalytics/{docId} {
            allow read: if signedIn();
            allow write: if isStaff();
        }

        match /restaurants/main/stockLogs/{docId} {
            allow read: if signedIn();
            allow write: if isStaff();
        }

        match /{document=**} {
            allow read, write: if false;
        }
    }
}
```

3. Click **Publish**

Alternative: use the project file `firestore.rules` and deploy with Firebase CLI:

```bash
firebase deploy --only firestore:rules
```

> Security Note: The rules above allow order placement for all signed-in users, but restrict inventory/menu/analytics writes to authorized staff emails only.

## Step 4: Get Firebase Config

1. Click Project Settings ⚙️ (top left)
2. Go to **Project settings**
3. Scroll to **Your apps** section
4. Click **Web** (</>) if not already selected
5. Copy the entire config object
6. Paste it in **firebase-config.js** replacing the placeholder config

Example config:
```javascript
const firebaseConfig = {
    apiKey: "AIzaSyDxxxxxxxxxxxxxxxxxxxxxxxx",
    authDomain: "kitchen-alert-12345.firebaseapp.com",
    projectId: "kitchen-alert-12345",
    storageBucket: "kitchen-alert-12345.appspot.com",
    messagingSenderId: "123456789",
    appId: "1:123456789:web:abcdefghijklmno"
};
```

## Step 5: Update HTML File

Add these scripts to **index.html** before the closing `</body>` tag:

```html
<!-- Firebase SDK -->
<script src="https://www.gstatic.com/firebaseapp/9.22.0/firebase-app.js"></script>
<script src="https://www.gstatic.com/firebaseapp/9.22.0/firebase-firestore.js"></script>
<script src="https://www.gstatic.com/firebaseapp/9.22.0/firebase-auth.js"></script>
<script src="https://www.gstatic.com/firebaseapp/9.22.0/firebase-database.js"></script>

<!-- Firebase Config & Integration -->
<script src="firebase-config.js"></script>

<script>
    // Initialize Firebase sync when page loads
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', function() {
            setTimeout(initFirebaseSync, 1000); // Wait for other scripts to load
        });
    } else {
        setTimeout(initFirebaseSync, 1000);
    }
</script>
```

## Step 6: Update Order Saving Function

Replace the `confirmOrder()` and `saveOrderToHistory()` functions to use Firebase:

```javascript
// In your code, update the confirmOrder function to include:
async function confirmOrder() {
    // ... existing code ...
    
    // Save to Firebase instead of just localStorage
    const orderData = {
        table: tableNumber,
        items: cartItems,
        total: grandTotal,
        time: duration
    };
    
    saveOrderToHistory(orderData);  // Saves to localStorage
    saveOrderToFirebase(orderData); // NEW: Also saves to Firebase
    
    // ... rest of code ...
}
```

## Step 7: Verify Data Structure in Firestore

After making your first order:

1. Go to Firebase Console
2. Click **Firestore Database**
3. You should see:
   ```
   restaurants/
     └── main/
         ├── inventory (all items and stock)
         ├── menu (all menu items)
         └── orders (all orders with timestamps)
   ```

## Troubleshooting

### Firebase Script Not Loading
- Check browser console (F12) for errors
- Verify CDN links are correct and not blocked

### Data Not Syncing
- Check Firebase rules are correct
- Verify config is correct (no typos in projectId, etc.)
- Check browser console for error messages

### Authentication Issues
- For development, security rules must allow anonymous reads/writes
- Add Firebase Authentication for production

## Real-Time Features Enabled

With this setup, you now have:

✅ **Real-time Inventory Sync** - Changes appear instantly on all devices
✅ **Persistent Order History** - Never lose order data
✅ **Multi-staff Support** - Multiple users can work simultaneously
✅ **Automatic Backups** - Firebase auto-backs up all data
✅ **Date-ranges Analytics** - Query orders from any date range
✅ **Offline Fallback** - Uses localStorage when offline, syncs when back online

## Next Steps

1. **Add Authentication**: Protect your data with staff login
2. **Enable Offline Persistence**: For better offline support
3. **Add Backup Rules**: Schedule automatic exports
4. **Set up Monitoring**: Get alerts for stock levels
