# Garage Assistant

Garage Assistant is a simple Flipper Zero external app for tracking basic motorcycle maintenance across multiple bikes. It uses Flipper's ViewPort, Canvas, input callback, and FuriMessageQueue APIs directly, without ViewDispatcher or Widget.

The app is currently implemented with the internal app id `garage_test`, so some build artifacts and storage paths still use that name.

## Features

- Multi-bike profiles, up to 5 bikes
- Add, rename, and delete bikes
- Prevents deleting the last remaining bike
- Unique default names for new bikes
- Editable oil and chain service intervals per bike
- Editable front and rear tire PSI per bike
- Service tracking for oil and chain mileage
- Miles remaining until service is due
- Reset oil and chain service counters
- Tire PSI reference page
- Persistent SD card save/load

## Controls

### Main Page

- `OK`: Switch to the next bike profile
- `Down`: Open Profile Edit
- `Right`: Open the Service page
- `Back`: Exit the app

### Profile Edit

- `Up` / `Down`: Move through menu items
- `OK`: Select an item
- `Back`: Return to the main page

Profile Edit includes:

- Rename Bike
- Edit Settings
- Add New Bike
- Delete Bike

### Rename Bike

- `Up` / `Down`: Change the selected character
- `Long Down`: Insert a space
- `Left` / `Right`: Move the cursor
- `OK`: Save the name
- `Back`: Cancel

### Edit Settings

- `Up` / `Down`: Move between settings
- `OK`: Edit the selected value
- `Back`: Return to Profile Edit

While editing a setting:

- `Up` / `Down`: Increase or decrease the value
- `OK`: Save
- `Back`: Cancel

### Delete Bike

- If only one bike exists, the app shows a message and does not allow deletion.
- If multiple bikes exist, choose a bike from the list with `Up` / `Down`.
- `OK`: Select the bike and open confirmation
- `Back`: Cancel and return to Profile Edit
- Deletion requires pressing `OK` twice across two confirmation screens.

### Service Page

- `Up` / `Down`: Select Oil or Chain
- `OK`: Edit the selected mileage counter
- `Right`: Open the Tire PSI page
- `Left`: Return to the main page

While editing a service counter:

- `Up` / `Down`: Adjust miles
- `Right`: Reset the selected counter to 0
- `OK`: Save
- `Back`: Cancel

### Tire PSI Page

- Shows the active bike's front and rear tire PSI reference values.
- `Left`: Return to the Service page

## Build

This folder is laid out as a standalone external app for the official Flipper Zero toolchain.

Install `ufbt`, then run:

```sh
ufbt
```

The built app is written to:

```text
dist/garage_test.fap
```

To build and launch directly to a connected Flipper Zero:

```sh
ufbt launch
```

## Install

After building, copy the `.fap` file to the Flipper Zero SD card:

```text
/ext/apps/Tools/
```

For example, copy:

```text
dist/garage_test.fap
```

to:

```text
/ext/apps/Tools/garage_test.fap
```

Then open the app from the Flipper Zero Apps menu under Tools.

## Data Storage

Garage Assistant saves profile and service data on the SD card at:

```text
/ext/apps_data/garage_test/garage_test.txt
```

The save file stores:

- Profile count
- Active profile index
- Bike names
- Oil and chain mileage counters
- Oil and chain service intervals
- Front and rear tire PSI values

## Persistent Bike Profiles

Bike profiles are saved automatically after changes such as switching bikes, renaming a bike, editing settings, updating service counters, resetting counters, adding a bike, or deleting a bike.

The app supports up to 5 profiles. It keeps profile indexes compact when a bike is deleted, prevents `profile_count` from dropping below 1, and keeps the active profile index valid after loading or deleting profiles.

## Current Limitations

- Maximum of 5 bike profiles
- Text entry is character-by-character
- Service tracking is mileage-based only
- No dates, notes, parts list, or maintenance history log yet
- No import/export UI for saved profile data
- The internal app id, file name, and data path still use `garage_test`

## Suggested Future Improvements

- Add service history entries with dates and notes
- Add more maintenance categories
- Add optional reminders based on time as well as mileage
- Add profile export/import
- Add clearer editing shortcuts for names and numeric values
- Rename internal ids and storage paths from `garage_test` to `garage_assistant` in a migration-safe way
