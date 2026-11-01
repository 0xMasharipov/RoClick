import {TurboModuleRegistry, type TurboModule} from 'react-native';
export type Locale = 'en' | 'tr';
export type UpdatePhase = 'unavailable' | 'checking' | 'current' | 'downloading' | 'ready' | 'error' | 'disabled' | 'otherInstance';
export type UpdateState = {enabled: boolean; state: UpdatePhase; version: string; availableVersion: string};
export interface Spec extends TurboModule {
  getLanguage(): Promise<Locale>;
  setLanguage(language: Locale): Promise<void>;
  getUpdateState(): Promise<UpdateState>;
  setAutomaticUpdates(enabled: boolean): Promise<void>;
  installUpdate(): Promise<void>;
  requestUpdateAttention(): void;
}
export default TurboModuleRegistry.getEnforcing<Spec>('AppSettings');
