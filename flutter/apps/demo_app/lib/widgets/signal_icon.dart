import 'package:flutter/material.dart';

/// A Wi-Fi signal-strength icon derived from an RSSI value (dBm).
class SignalIcon extends StatelessWidget {
  const SignalIcon(this.rssi, {super.key});

  final int rssi;

  IconData get _icon {
    if (rssi >= -55) return Icons.signal_wifi_4_bar;
    if (rssi >= -65) return Icons.network_wifi_3_bar;
    if (rssi >= -75) return Icons.network_wifi_2_bar;
    if (rssi >= -85) return Icons.network_wifi_1_bar;
    return Icons.signal_wifi_0_bar;
  }

  @override
  Widget build(BuildContext context) => Icon(_icon);
}
