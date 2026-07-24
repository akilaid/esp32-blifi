import 'dart:async';

import 'package:flutter/material.dart';
import 'package:mobile_scanner/mobile_scanner.dart';

import '../provisioning_controller.dart';
import '../qr_payload.dart';
import '../widgets/viewfinder_overlay.dart';

/// Camera QR scanner. On a valid blifi code, auto-connects.
///
/// mobile_scanner v7 no longer auto-starts the camera for you, so we drive the
/// controller lifecycle explicitly (start on resume, stop when backgrounded)
/// and surface any camera-start failure through [MobileScanner.errorBuilder]
/// instead of the plugin's bare error icon.
class QrScanScreen extends StatefulWidget {
  const QrScanScreen(this.controller, {super.key});

  final ProvisioningController controller;

  @override
  State<QrScanScreen> createState() => _QrScanScreenState();
}

class _QrScanScreenState extends State<QrScanScreen> with WidgetsBindingObserver {
  final MobileScannerController _scanner =
      MobileScannerController(autoStart: false);
  StreamSubscription<BarcodeCapture>? _subscription;
  bool _handled = false;
  String? _hint;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _subscription = _scanner.barcodes.listen(_onDetect);
    unawaited(_scanner.start());
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (!_scanner.value.hasCameraPermission) return;
    switch (state) {
      case AppLifecycleState.resumed:
        _subscription ??= _scanner.barcodes.listen(_onDetect);
        unawaited(_scanner.start());
      case AppLifecycleState.inactive:
        unawaited(_subscription?.cancel());
        _subscription = null;
        unawaited(_scanner.stop());
      default:
        return;
    }
  }

  @override
  Future<void> dispose() async {
    WidgetsBinding.instance.removeObserver(this);
    unawaited(_subscription?.cancel());
    _subscription = null;
    super.dispose();
    await _scanner.dispose();
  }

  void _onDetect(BarcodeCapture capture) {
    if (_handled) return;
    for (final barcode in capture.barcodes) {
      final raw = barcode.rawValue;
      if (raw == null) continue;
      final payload = QrPayload.tryParse(raw);
      if (payload != null) {
        _handled = true;
        unawaited(_scanner.stop());
        widget.controller.connectViaQr(payload.deviceName, payload.pop);
        return;
      }
    }
    if (mounted) setState(() => _hint = 'That code isn’t a blifi device code.');
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      extendBodyBehindAppBar: true,
      appBar: AppBar(
        backgroundColor: Colors.transparent,
        foregroundColor: Colors.white,
        title: const Text('Scan device QR'),
        leading: IconButton(
          icon: const Icon(Icons.arrow_back_rounded),
          onPressed: widget.controller.startOver,
        ),
        actions: [
          IconButton(
            icon: const Icon(Icons.flash_on_rounded),
            onPressed: () => _scanner.toggleTorch(),
          ),
        ],
      ),
      body: Stack(
        fit: StackFit.expand,
        children: [
          MobileScanner(
            controller: _scanner,
            errorBuilder: (context, error) => _CameraError(
              error: error,
              onRetry: () => _scanner.start(),
              onManual: widget.controller.startScan,
            ),
          ),
          Container(color: Colors.black.withValues(alpha: 0.25)),
          const ViewfinderOverlay(),
          Positioned(
            left: 24,
            right: 24,
            bottom: 48,
            child: Column(
              children: [
                Text(
                  _hint ?? 'Point the camera at the QR code on your device.',
                  textAlign: TextAlign.center,
                  style: const TextStyle(color: Colors.white, fontSize: 15),
                ),
                const SizedBox(height: 16),
                TextButton.icon(
                  style: TextButton.styleFrom(foregroundColor: Colors.white),
                  onPressed: widget.controller.startScan,
                  icon: const Icon(Icons.keyboard_rounded),
                  label: const Text('Enter manually instead'),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

/// Friendly fallback when the camera can't be opened — shows the real error code
/// (so failures are diagnosable) plus retry and manual-entry escapes.
class _CameraError extends StatelessWidget {
  const _CameraError({
    required this.error,
    required this.onRetry,
    required this.onManual,
  });

  final MobileScannerException error;
  final VoidCallback onRetry;
  final VoidCallback onManual;

  @override
  Widget build(BuildContext context) {
    final denied = error.errorCode == MobileScannerErrorCode.permissionDenied;
    final title = denied ? 'Camera permission needed' : 'Couldn’t open the camera';
    final detail = denied
        ? 'Allow camera access in Settings, or enter the device manually.'
        : 'You can retry, or enter the device manually.';
    return ColoredBox(
      color: Colors.black,
      child: Center(
        child: Padding(
          padding: const EdgeInsets.all(32),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const Icon(Icons.no_photography_rounded,
                  color: Colors.white, size: 48),
              const SizedBox(height: 16),
              Text(
                title,
                textAlign: TextAlign.center,
                style: const TextStyle(
                    color: Colors.white,
                    fontSize: 20,
                    fontWeight: FontWeight.w600),
              ),
              const SizedBox(height: 8),
              Text(
                detail,
                textAlign: TextAlign.center,
                style: const TextStyle(color: Colors.white70, fontSize: 14),
              ),
              const SizedBox(height: 8),
              Text(
                error.errorCode.name,
                textAlign: TextAlign.center,
                style: const TextStyle(color: Colors.white38, fontSize: 12),
              ),
              const SizedBox(height: 24),
              if (!denied)
                FilledButton.icon(
                  onPressed: onRetry,
                  icon: const Icon(Icons.refresh_rounded),
                  label: const Text('Retry'),
                ),
              TextButton.icon(
                style: TextButton.styleFrom(foregroundColor: Colors.white),
                onPressed: onManual,
                icon: const Icon(Icons.keyboard_rounded),
                label: const Text('Enter manually instead'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
