import {TurboModuleRegistry, type TurboModule} from 'react-native';
import type {EventEmitter} from 'react-native/Libraries/Types/CodegenTypes';

export type MouseButton = 'left' | 'right' | 'middle';
export type Theme = 'system' | 'light' | 'dark';
export type ClickConfig = {intervalMs: number; pressMs: number; button: MouseButton; lockToTarget: boolean};
export type EngineState = {
  state: 'stopped' | 'running' | 'paused' | 'capturing' | 'error';
  config: ClickConfig;
  targetTitle: string; targetPid: number;
  submittedClicks: number; submittedEvents: number; submittedRate: number;
  skipped: number; failures: number; error: string; errorCode: number; errorKey: '' | 'invalidConfig' | 'targetClosed' | 'captureFailed' | 'inputRejected' | 'releaseRejected' | 'targetMissing' | 'monitoringUnavailable' | 'timerUnavailable' | 'schedulerWaitFailed'; held: boolean;
  highResolution: boolean; acrylic: boolean; dark: boolean; highContrast: boolean;
  windowColor: string; textColor: string; buttonColor: string; buttonTextColor: string;
  highlightColor: string; highlightTextColor: string;
};
export interface Spec extends TurboModule {
  configure(config: ClickConfig): void;
  start(): void;
  stop(): void;
  captureTarget(delayMs: number): void;
  clearTarget(): void;
  setTheme(theme: Theme): void;
  observe(enabled: boolean): void;
  acknowledge(): void;
  readonly onState: EventEmitter<EngineState>;
}
export default TurboModuleRegistry.getEnforcing<Spec>('ClickEngine');
