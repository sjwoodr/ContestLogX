# Quickstart: Cloud Storage for Contest Logs

**Feature**: 005-cloud-storage

## For operators: configure FileLu and save a log to the cloud

1. Create a FileLu account at https://filelu.com (developer offer: up to 200 GB free lifetime for
   tools that integrate FileLu).
2. In FileLu → My Account, enable an S5 bucket and generate an **Access Key** and **Secret Key**.
3. In ContestLogX → **Preferences → Cloud Storage → FileLu**:
   - Check **Enabled**.
   - Endpoint: `https://s5lu.com` (pre-filled). Region: `global` (pre-filled).
   - Bucket: your S5 bucket name.
   - Access Key / Secret Key: paste the values from FileLu.
   - Click **Test connection** → expect "Connection OK".
   - Click **OK**.
4. **Save a log to the cloud**: File → Save. Because a provider is configured, a destination
   chooser appears - pick **FileLu**, confirm the filename, Save. The log uploads.
5. **Open a log from the cloud**: File → Open → choose **FileLu** → pick a `.clx` log from the
   list → Open. It downloads and loads with all QSOs/score intact.

> If you configure **no** cloud providers, Open/Save behave exactly as before - straight to the
> local file dialog, no extra step.

## For operators: AWS S3

Same as above, but in **Preferences → Cloud Storage → AWS S3**: set the regional endpoint
(e.g. `https://s3.us-east-1.amazonaws.com`), region (e.g. `us-east-1`), bucket, and your AWS
access/secret keys.

## Stub providers

Dropbox, Google Drive, and iCloud Drive appear in the Cloud Storage settings but are marked
**"Not implemented yet"** with disabled inputs.

## For developers: build & test

```bash
make                 # zero-warning build (GCC/Clang); links existing Qt6 Network + Xml - no new deps
make test            # unit tests, including the SigV4 signer test
make test-logs       # contest log validation (must remain green - engine untouched)
```

### Verifying the SigV4 signer

The signer is isolated in `s3Signer` (no Qt network types). Its unit test feeds an **AWS published
SigV4 test vector** (known canonical request + expected `Authorization`) and asserts an exact match.
Run it via `make test`.

### Live smoke test (manual, pre-merge)

With valid FileLu keys configured, save a small log to FileLu and re-open it; confirm byte-identical
round-trip. This validates real SigV4 + transport against the `s5lu.com` endpoint.

## Where things live

| Concern | File |
|---------|------|
| Provider interface | `include/net/cloudStorageProvider.h` |
| S3 provider (facade + worker) | `include/net/s3StorageProvider.h`, `s3StorageWorker.h` / `src/net/*` |
| SigV4 signer | `include/net/s3Signer.h`, `src/net/s3Signer.cpp` |
| Settings | `src/utils/settings.cpp` (`cloudStorage` section), `include/settings.h` |
| Preferences UI | `src/ui/preferencesDialog.cpp` (Cloud Storage tab) |
| Open/Save integration | `src/ui/mainWindow.cpp` (`onOpenLog`, `onSaveLog`) |
