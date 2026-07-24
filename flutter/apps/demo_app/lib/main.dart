import 'package:dynamic_color/dynamic_color.dart';
import 'package:flutter/material.dart';

import 'provisioning_controller.dart';
import 'screens/connecting_screen.dart';
import 'screens/home_screen.dart';
import 'screens/permission_screen.dart';
import 'screens/progress_screen.dart';
import 'screens/qr_scan_screen.dart';
import 'screens/result_screen.dart';
import 'screens/scan_screen.dart';
import 'screens/wifi_screen.dart';

void main() => runApp(const BlifiDemoApp());

/// A generic, reusable Material 3 Expressive demo of BLE Wi-Fi provisioning.
class BlifiDemoApp extends StatefulWidget {
  const BlifiDemoApp({super.key});

  @override
  State<BlifiDemoApp> createState() => _BlifiDemoAppState();
}

class _BlifiDemoAppState extends State<BlifiDemoApp> {
  final _controller = ProvisioningController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  ThemeData _theme(ColorScheme scheme) => ThemeData(
        colorScheme: scheme,
        useMaterial3: true,
        // Opt into the expressive (2024+) progress indicators app-wide.
        // ignore: deprecated_member_use
        progressIndicatorTheme: const ProgressIndicatorThemeData(year2023: false),
        appBarTheme: const AppBarTheme(centerTitle: false),
        cardTheme: CardThemeData(
          clipBehavior: Clip.antiAlias,
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(24)),
        ),
        filledButtonTheme: FilledButtonThemeData(
          style: FilledButton.styleFrom(
            minimumSize: const Size.fromHeight(56),
            shape: const StadiumBorder(),
            textStyle: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
          ),
        ),
      );

  @override
  Widget build(BuildContext context) {
    const seed = Color(0xFF4F5BD5); // expressive indigo, used if no dynamic color
    return DynamicColorBuilder(
      builder: (ColorScheme? light, ColorScheme? dark) {
        final lightScheme = light ?? ColorScheme.fromSeed(seedColor: seed);
        final darkScheme = dark ??
            ColorScheme.fromSeed(seedColor: seed, brightness: Brightness.dark);
        return MaterialApp(
          title: 'blifi',
          debugShowCheckedModeBanner: false,
          theme: _theme(lightScheme),
          darkTheme: _theme(darkScheme),
          themeMode: ThemeMode.system,
          home: _Router(_controller),
        );
      },
    );
  }
}

/// Renders the current stage with a smooth fade-through transition.
class _Router extends StatelessWidget {
  const _Router(this.controller);
  final ProvisioningController controller;

  Widget _screenFor(ProvisioningStage stage) => switch (stage) {
        ProvisioningStage.permissions => PermissionScreen(controller),
        ProvisioningStage.home => HomeScreen(controller),
        ProvisioningStage.qrScan => QrScanScreen(controller),
        ProvisioningStage.scanning => ScanScreen(controller),
        ProvisioningStage.connecting => ConnectingScreen(controller),
        ProvisioningStage.wifiList => WifiScreen(controller),
        ProvisioningStage.provisioning => ProgressScreen(controller),
        ProvisioningStage.success => ResultScreen(controller, success: true),
        ProvisioningStage.failure => ResultScreen(controller, success: false),
      };

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: controller,
      builder: (context, _) {
        final stage = controller.stage;
        return AnimatedSwitcher(
          duration: const Duration(milliseconds: 420),
          switchInCurve: Curves.easeOutCubic,
          switchOutCurve: Curves.easeInCubic,
          transitionBuilder: (child, animation) => FadeTransition(
            opacity: animation,
            child: SlideTransition(
              position: Tween<Offset>(
                begin: const Offset(0, 0.03),
                end: Offset.zero,
              ).animate(animation),
              child: child,
            ),
          ),
          child: KeyedSubtree(
            key: ValueKey(stage),
            child: _screenFor(stage),
          ),
        );
      },
    );
  }
}
