import 'package:blifi/blifi.dart';
import 'package:flutter/material.dart';

import '../provisioning_controller.dart';

/// Live provisioning progress while the device joins the network.
class ProgressScreen extends StatelessWidget {
  const ProgressScreen(this.controller, {super.key});

  final ProvisioningController controller;

  String get _label => switch (controller.status?.state) {
        ProvisioningState.credentialsReceived => 'Credentials received…',
        ProvisioningState.wifiConnecting => 'Connecting to Wi-Fi…',
        _ => 'Sending credentials…',
      };

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(controller.connectedSsid ?? 'Provisioning')),
      body: Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const CircularProgressIndicator(),
            const SizedBox(height: 24),
            Text(_label, style: Theme.of(context).textTheme.titleMedium),
          ],
        ),
      ),
    );
  }
}
