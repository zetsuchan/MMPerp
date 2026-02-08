import { ImGui, ImGuiImplWeb, ImVec2, ImVec4 } from '@mori2003/jsimgui';

import './style.css';

type BookLevel = {
  price: number;
  size: number;
  total: number;
};

type TradePrint = {
  side: 'BUY' | 'SELL';
  price: number;
  size: number;
  age: number;
};

type ActivityEvent = {
  level: 'info' | 'success' | 'warn';
  text: string;
  age: number;
};

type AppState = {
  market: [number];
  timeframe: [number];
  orderType: [number];
  side: [number];
  leverage: [number];
  amount: [number];
  limitPrice: [number];
  maxPerTrade: [number];
  dailyCap: [number];
  agentEnabled: [boolean];
  autoExecute: [boolean];
  strictGuards: [boolean];
  confidence: [number];
  prompt: [string];
  notes: [string];
  chart: number[];
  markPrice: number;
  spread: number;
  bids: BookLevel[];
  asks: BookLevel[];
  trades: TradePrint[];
  events: ActivityEvent[];
  pnlToday: number;
  equity: number;
  uptimeSeconds: number;
  cycles: number;
  responseMs: number;
  nextTradeIn: number;
  nextEventIn: number;
  currentSignal: 'BULLISH' | 'BEARISH' | 'NEUTRAL';
};

const PAIRS = ['ETH/USD PERP', 'BTC/USD PERP', 'SOL/USD PERP'];
const PAIRS_IMGUI = `${PAIRS.join('\0')}\0\0`;
const TIMEFRAMES = ['1m', '5m', '15m', '1h', '4h'];
const ORDER_TYPES = ['Market', 'Limit', 'TWAP'];
const ORDER_TYPES_IMGUI = `${ORDER_TYPES.join('\0')}\0\0`;
const SIDES = ['Buy', 'Sell'];
const SIDES_IMGUI = `${SIDES.join('\0')}\0\0`;

const GREEN = new ImVec4(0.34, 0.88, 0.43, 1);
const RED = new ImVec4(0.94, 0.34, 0.37, 1);
const AMBER = new ImVec4(0.98, 0.75, 0.29, 1);
const DIM = new ImVec4(0.56, 0.59, 0.67, 1);

const clamp = (value: number, min: number, max: number): number =>
  Math.min(max, Math.max(min, value));

const formatUsd = (value: number): string =>
  `$${value.toLocaleString('en-US', {
    maximumFractionDigits: 2,
    minimumFractionDigits: 2,
  })}`;

const formatQty = (value: number): string =>
  value.toLocaleString('en-US', { maximumFractionDigits: 4, minimumFractionDigits: 2 });

const formatAge = (seconds: number): string => {
  const m = Math.floor(seconds / 60);
  const s = Math.floor(seconds % 60);
  return `${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
};

const bootstrapChart = (start: number, points: number): number[] => {
  const out: number[] = [];
  let value = start;
  for (let i = 0; i < points; i += 1) {
    value += (Math.random() - 0.5) * 8;
    out.push(value);
  }
  return out;
};

const createBook = (
  markPrice: number,
): { bids: BookLevel[]; asks: BookLevel[]; spread: number } => {
  const levels = 15;
  const tick = markPrice * 0.0002;
  const spread = tick * (1.3 + Math.random() * 1.4);
  const bids: BookLevel[] = [];
  const asks: BookLevel[] = [];
  let runningBid = 0;
  let runningAsk = 0;

  for (let i = 0; i < levels; i += 1) {
    const bidSize = 0.2 + Math.random() * 3;
    runningBid += bidSize;
    bids.push({
      price: markPrice - spread - i * tick,
      size: bidSize,
      total: runningBid,
    });

    const askSize = 0.2 + Math.random() * 3;
    runningAsk += askSize;
    asks.push({
      price: markPrice + spread + i * tick,
      size: askSize,
      total: runningAsk,
    });
  }

  return { bids, asks, spread: spread * 2 };
};

const nowEvent = (text: string, level: ActivityEvent['level']): ActivityEvent => ({
  text,
  level,
  age: 0,
});

const initState = (): AppState => {
  const markPrice = 3232.07;
  const book = createBook(markPrice);
  return {
    market: [0],
    timeframe: [1],
    orderType: [1],
    side: [0],
    leverage: [8],
    amount: [0.12],
    limitPrice: [markPrice],
    maxPerTrade: [0.01],
    dailyCap: [0.1],
    agentEnabled: [true],
    autoExecute: [false],
    strictGuards: [true],
    confidence: [0.75],
    prompt: ['Watch CVD divergence + sweep probability around local highs.'],
    notes: ['Connected to monmouth_nodeStatus. Waiting for L2 bridge warmup...'],
    chart: bootstrapChart(markPrice, 220),
    markPrice,
    spread: book.spread,
    bids: book.bids,
    asks: book.asks,
    trades: [],
    events: [
      nowEvent('OpenClaw agent connected', 'success'),
      nowEvent('Syncing strategy profile', 'info'),
      nowEvent('Budget guards active', 'info'),
    ],
    pnlToday: 12.45,
    equity: 1224.22,
    uptimeSeconds: 3600,
    cycles: 12,
    responseMs: 850,
    nextTradeIn: 0.7,
    nextEventIn: 3.4,
    currentSignal: 'BULLISH',
  };
};

const pushEvent = (state: AppState, text: string, level: ActivityEvent['level']): void => {
  state.events.unshift(nowEvent(text, level));
  if (state.events.length > 60) {
    state.events.length = 60;
  }
};

const pushTrade = (state: AppState): void => {
  const side = Math.random() > 0.5 ? 'BUY' : 'SELL';
  const move = (Math.random() - 0.5) * state.spread * 2.5;
  const price = state.markPrice + move;
  const size = 0.08 + Math.random() * 2.6;
  state.trades.unshift({ side, price, size, age: 0 });
  if (state.trades.length > 80) {
    state.trades.length = 80;
  }
};

const updateState = (state: AppState, deltaSeconds: number): void => {
  state.uptimeSeconds += deltaSeconds;
  state.nextTradeIn -= deltaSeconds;
  state.nextEventIn -= deltaSeconds;

  // Simulated mark and PnL feed for local mock mode.
  const drift = Math.sin(state.uptimeSeconds * 0.14) * 1.6;
  const noise = (Math.random() - 0.5) * 7.5;
  state.markPrice = clamp(state.markPrice + drift + noise, 1200, 12000);
  state.limitPrice[0] = state.orderType[0] === 1 ? state.markPrice : state.limitPrice[0];

  state.pnlToday = clamp(state.pnlToday + (Math.random() - 0.49) * 1.2, -140, 220);
  state.equity = clamp(state.equity + (Math.random() - 0.5) * 3.6, 300, 8000);
  state.responseMs = clamp(state.responseMs + (Math.random() - 0.5) * 20, 520, 1400);

  state.chart.push(state.markPrice);
  if (state.chart.length > 300) {
    state.chart.shift();
  }

  const book = createBook(state.markPrice);
  state.bids = book.bids;
  state.asks = book.asks;
  state.spread = book.spread;

  for (const trade of state.trades) {
    trade.age += deltaSeconds;
  }
  for (const event of state.events) {
    event.age += deltaSeconds;
  }

  if (state.nextTradeIn <= 0) {
    pushTrade(state);
    state.nextTradeIn = 0.35 + Math.random() * 1.4;
  }

  if (state.nextEventIn <= 0) {
    const rolls = [
      ['Waiting for L2 connection...', 'warn'],
      ['Research agent finished policy check', 'success'],
      ['Guardrail test order simulated', 'info'],
      ['Risk check passed for ETH/USD PERP', 'success'],
      ['Observed short-term sell pressure', 'warn'],
    ] as const;
    const pick = rolls[Math.floor(Math.random() * rolls.length)];
    pushEvent(state, pick[0], pick[1]);
    state.nextEventIn = 2.8 + Math.random() * 4.1;
  }

  if (Math.random() < 0.004) {
    state.currentSignal = Math.random() > 0.5 ? 'BULLISH' : 'BEARISH';
  } else if (Math.random() < 0.003) {
    state.currentSignal = 'NEUTRAL';
  }
};

const applyTheme = (): void => {
  ImGui.StyleColorsDark();
  const style = ImGui.GetStyle();
  style.WindowRounding = 7;
  style.ChildRounding = 7;
  style.FrameRounding = 6;
  style.ScrollbarRounding = 8;
  style.GrabRounding = 4;
  style.FrameBorderSize = 1;
  style.WindowBorderSize = 1;
  style.ItemSpacing = new ImVec2(8, 8);
  style.WindowPadding = new ImVec2(10, 10);

  const colors = style.Colors;
  colors[ImGui.Col.WindowBg] = new ImVec4(0.03, 0.04, 0.06, 1);
  colors[ImGui.Col.ChildBg] = new ImVec4(0.05, 0.06, 0.09, 1);
  colors[ImGui.Col.FrameBg] = new ImVec4(0.09, 0.1, 0.15, 1);
  colors[ImGui.Col.FrameBgHovered] = new ImVec4(0.12, 0.14, 0.2, 1);
  colors[ImGui.Col.FrameBgActive] = new ImVec4(0.13, 0.16, 0.22, 1);
  colors[ImGui.Col.TitleBg] = new ImVec4(0.05, 0.06, 0.09, 1);
  colors[ImGui.Col.TitleBgActive] = new ImVec4(0.08, 0.1, 0.14, 1);
  colors[ImGui.Col.Header] = new ImVec4(0.1, 0.2, 0.14, 0.8);
  colors[ImGui.Col.HeaderHovered] = new ImVec4(0.14, 0.29, 0.2, 1);
  colors[ImGui.Col.HeaderActive] = new ImVec4(0.16, 0.33, 0.23, 1);
  colors[ImGui.Col.Button] = new ImVec4(0.11, 0.24, 0.17, 0.9);
  colors[ImGui.Col.ButtonHovered] = new ImVec4(0.15, 0.33, 0.23, 1);
  colors[ImGui.Col.ButtonActive] = new ImVec4(0.18, 0.39, 0.27, 1);
  colors[ImGui.Col.CheckMark] = new ImVec4(0.43, 0.9, 0.5, 1);
  colors[ImGui.Col.Separator] = new ImVec4(0.2, 0.25, 0.33, 0.8);
  colors[ImGui.Col.Tab] = new ImVec4(0.08, 0.09, 0.14, 1);
  colors[ImGui.Col.TabHovered] = new ImVec4(0.17, 0.2, 0.31, 1);
  colors[ImGui.Col.TabSelected] = new ImVec4(0.13, 0.17, 0.27, 1);
  colors[ImGui.Col.PlotLines] = new ImVec4(0.45, 0.9, 0.52, 1);
  colors[ImGui.Col.PlotLinesHovered] = new ImVec4(0.86, 0.97, 0.47, 1);
};

const beginPinnedWindow = (
  title: string,
  posX: number,
  posY: number,
  width: number,
  height: number,
  flags = 0,
): boolean => {
  ImGui.SetNextWindowPos(new ImVec2(posX, posY), ImGui.Cond.Always);
  ImGui.SetNextWindowSize(new ImVec2(width, height), ImGui.Cond.Always);
  const lockFlags =
    ImGui.WindowFlags.NoMove | ImGui.WindowFlags.NoResize | ImGui.WindowFlags.NoCollapse;
  return ImGui.Begin(title, null, lockFlags | flags);
};

const renderHeaderBar = (state: AppState): void => {
  if (!ImGui.BeginMainMenuBar()) {
    return;
  }
  ImGui.Text('MMPerp | Monmouth Perp Terminal');
  ImGui.Separator();
  ImGui.TextColored(DIM, `Pair ${PAIRS[state.market[0]]}`);
  ImGui.SameLine(0, 16);
  ImGui.TextColored(state.pnlToday >= 0 ? GREEN : RED, `Today ${formatUsd(state.pnlToday)}`);
  ImGui.SameLine(0, 16);
  ImGui.TextColored(DIM, `Latency ${state.responseMs.toFixed(0)}ms`);
  ImGui.SameLine(0, 16);
  ImGui.TextColored(DIM, `Uptime ${formatAge(state.uptimeSeconds)}`);

  if (ImGui.BeginMenu('Mode')) {
    ImGui.MenuItem('Mock Market Feed', '', true, false);
    ImGui.MenuItem('Live Gateway (planned)', '', false, false);
    ImGui.EndMenu();
  }
  if (ImGui.BeginMenu('Agent')) {
    if (ImGui.MenuItem('Toggle Agent', 'A')) {
      state.agentEnabled[0] = !state.agentEnabled[0];
    }
    if (ImGui.MenuItem('Auto-Execute', '', state.autoExecute[0])) {
      state.autoExecute[0] = !state.autoExecute[0];
    }
    ImGui.EndMenu();
  }
  ImGui.EndMainMenuBar();
};

const renderChartSection = (state: AppState): void => {
  ImGui.TextColored(DIM, 'Perp market');
  ImGui.SameLine();
  ImGui.Combo('##market', state.market, PAIRS_IMGUI);
  ImGui.SameLine();
  ImGui.TextColored(GREEN, formatUsd(state.markPrice));
  ImGui.SameLine();
  const spreadBps = (state.spread / state.markPrice) * 10000;
  ImGui.TextColored(DIM, `Spread ${spreadBps.toFixed(2)} bps`);

  for (let i = 0; i < TIMEFRAMES.length; i += 1) {
    if (i > 0) {
      ImGui.SameLine();
    }
    const selected = state.timeframe[0] === i;
    if (selected) {
      ImGui.PushStyleColorImVec4(ImGui.Col.Button, new ImVec4(0.21, 0.43, 0.29, 0.9));
    }
    if (ImGui.Button(TIMEFRAMES[i])) {
      state.timeframe[0] = i;
    }
    if (selected) {
      ImGui.PopStyleColor();
    }
  }

  const area = ImGui.GetContentRegionAvail();
  const chartHeight = Math.max(180, area.y * 0.56);
  if (ImGui.BeginChild('ChartCanvas', new ImVec2(0, chartHeight), ImGui.ChildFlags.Borders)) {
    const minimum = Math.min(...state.chart);
    const maximum = Math.max(...state.chart);
    ImGui.PlotLines(
      '##price-series',
      state.chart,
      state.chart.length,
      0,
      `${PAIRS[state.market[0]]} ${formatUsd(state.markPrice)}`,
      minimum * 0.995,
      maximum * 1.005,
      new ImVec2(-1, -1),
    );
    ImGui.TextColored(
      DIM,
      `H ${formatUsd(maximum)}    L ${formatUsd(minimum)}    Last ${formatUsd(state.markPrice)}`,
    );
  }
  ImGui.EndChild();
};

const renderOrderBook = (state: AppState): void => {
  const tableFlags =
    ImGui.TableFlags.RowBg | ImGui.TableFlags.Borders | ImGui.TableFlags.SizingStretchSame;
  if (!ImGui.BeginTable('OrderBookTable', 3, tableFlags, new ImVec2(0, 0))) {
    return;
  }
  ImGui.TableSetupColumn('Price');
  ImGui.TableSetupColumn('Size');
  ImGui.TableSetupColumn('Total');
  ImGui.TableHeadersRow();

  for (let i = state.asks.length - 1; i >= 0; i -= 1) {
    const row = state.asks[i];
    ImGui.TableNextRow();
    ImGui.TableNextColumn();
    ImGui.TextColored(RED, row.price.toFixed(2));
    ImGui.TableNextColumn();
    ImGui.Text(formatQty(row.size));
    ImGui.TableNextColumn();
    ImGui.Text(formatQty(row.total));
  }

  ImGui.TableNextRow();
  ImGui.TableNextColumn();
  ImGui.TextColored(AMBER, `Spread ${state.spread.toFixed(2)}`);
  ImGui.TableNextColumn();
  ImGui.TextDisabled(' ');
  ImGui.TableNextColumn();
  ImGui.TextDisabled(' ');

  for (const row of state.bids) {
    ImGui.TableNextRow();
    ImGui.TableNextColumn();
    ImGui.TextColored(GREEN, row.price.toFixed(2));
    ImGui.TableNextColumn();
    ImGui.Text(formatQty(row.size));
    ImGui.TableNextColumn();
    ImGui.Text(formatQty(row.total));
  }

  ImGui.EndTable();
};

const renderTape = (state: AppState): void => {
  const tableFlags =
    ImGui.TableFlags.RowBg | ImGui.TableFlags.Borders | ImGui.TableFlags.SizingStretchSame;
  if (!ImGui.BeginTable('TapeTable', 4, tableFlags, new ImVec2(0, 0))) {
    return;
  }
  ImGui.TableSetupColumn('Side');
  ImGui.TableSetupColumn('Price');
  ImGui.TableSetupColumn('Size');
  ImGui.TableSetupColumn('Age');
  ImGui.TableHeadersRow();

  for (const trade of state.trades.slice(0, 28)) {
    ImGui.TableNextRow();
    ImGui.TableNextColumn();
    ImGui.TextColored(trade.side === 'BUY' ? GREEN : RED, trade.side);
    ImGui.TableNextColumn();
    ImGui.Text(trade.price.toFixed(2));
    ImGui.TableNextColumn();
    ImGui.Text(formatQty(trade.size));
    ImGui.TableNextColumn();
    ImGui.Text(`${trade.age.toFixed(1)}s`);
  }

  ImGui.EndTable();
};

const renderMarketWindow = (state: AppState, x: number, y: number, w: number, h: number): void => {
  if (!beginPinnedWindow('Market + Book', x, y, w, h)) {
    ImGui.End();
    return;
  }

  renderChartSection(state);
  if (ImGui.BeginTabBar('MarketTabs')) {
    if (ImGui.BeginTabItem('Order Book')) {
      renderOrderBook(state);
      ImGui.EndTabItem();
    }
    if (ImGui.BeginTabItem('Trades')) {
      renderTape(state);
      ImGui.EndTabItem();
    }
    ImGui.EndTabBar();
  }

  ImGui.End();
};

const renderActivityWindow = (
  state: AppState,
  x: number,
  y: number,
  w: number,
  h: number,
): void => {
  if (!beginPinnedWindow('Activity Feed', x, y, w, h)) {
    ImGui.End();
    return;
  }

  ImGui.TextColored(DIM, `${state.events.length} events | live`);
  ImGui.Separator();
  if (ImGui.BeginChild('ActivityList', new ImVec2(0, 0), 0)) {
    for (const event of state.events.slice(0, 24)) {
      const color = event.level === 'success' ? GREEN : event.level === 'warn' ? AMBER : DIM;
      ImGui.TextColored(color, `(${event.age.toFixed(1)}s) ${event.text}`);
    }
  }
  ImGui.EndChild();
  ImGui.End();
};

const renderTraderAgentWindow = (
  state: AppState,
  x: number,
  y: number,
  w: number,
  h: number,
): void => {
  if (!beginPinnedWindow('Trader Agent', x, y, w, h)) {
    ImGui.End();
    return;
  }

  ImGui.TextColored(DIM, state.agentEnabled[0] ? 'status: connected' : 'status: idle');
  ImGui.Text(`did:key:7099...17dc79C8`);
  ImGui.Separator();
  ImGui.Text(`Balance  ${formatUsd(state.equity)}`);
  ImGui.TextColored(state.pnlToday >= 0 ? GREEN : RED, `Today's PnL  ${formatUsd(state.pnlToday)}`);
  ImGui.Text(`Trades  ${state.cycles}`);
  ImGui.SeparatorText('Policy');

  ImGui.SliderFloat('Max / Trade (ETH)', state.maxPerTrade, 0.001, 0.05, '%.3f');
  ImGui.SliderFloat('Daily Cap (ETH)', state.dailyCap, 0.01, 1.0, '%.2f');
  ImGui.SliderFloat('Confidence Gate', state.confidence, 0.5, 0.95, '%.2f');
  ImGui.Checkbox('Enable Agent', state.agentEnabled);
  ImGui.Checkbox('Auto Execute', state.autoExecute);
  ImGui.Checkbox('Strict Guardrails', state.strictGuards);

  const usage = clamp(Math.abs(state.pnlToday) / Math.max(state.dailyCap[0] * 1000, 1), 0, 1);
  ImGui.ProgressBar(usage, new ImVec2(-1, 0), `Risk Utilization ${(usage * 100).toFixed(0)}%`);

  if (ImGui.Button('Run Agent Cycle')) {
    state.cycles += 1;
    pushEvent(state, 'Agent cycle executed', 'success');
  }
  ImGui.SameLine();
  if (ImGui.Button('Pause')) {
    state.agentEnabled[0] = false;
    pushEvent(state, 'Agent paused by operator', 'warn');
  }

  ImGui.End();
};

const renderResearchWindow = (
  state: AppState,
  x: number,
  y: number,
  w: number,
  h: number,
): void => {
  if (!beginPinnedWindow('Research Agent', x, y, w, h)) {
    ImGui.End();
    return;
  }

  ImGui.TextColored(DIM, 'available');
  ImGui.Text('did:key:3C44cDD...FA4293BC');
  ImGui.Separator();
  ImGui.Text(`Signal ${state.currentSignal}`);
  ImGui.Text(`Escrow Fill Rate 92%`);
  ImGui.Text(`Avg Response ${state.responseMs.toFixed(0)}ms`);
  ImGui.SeparatorText('Reasoning');

  ImGui.InputTextMultiline('##analysis', state.notes, 4096, new ImVec2(-1, 110));
  ImGui.InputTextWithHint('##prompt', 'next analysis prompt', state.prompt, 2048);

  if (ImGui.Button('Generate Thesis')) {
    pushEvent(state, `Research thesis updated (${state.currentSignal.toLowerCase()})`, 'success');
  }
  ImGui.SameLine();
  if (ImGui.Button('Queue Test Order')) {
    pushEvent(state, 'Queued guardrailed test order', 'info');
  }

  ImGui.End();
};

const renderSidecarWindow = (
  state: AppState,
  x: number,
  y: number,
  w: number,
  h: number,
  frameRate: number,
): void => {
  if (!beginPinnedWindow('OpenClaw Agent', x, y, w, h)) {
    ImGui.End();
    return;
  }

  ImGui.TextColored(DIM, state.agentEnabled[0] ? 'connected' : 'paused');
  ImGui.SeparatorText('Identity');
  ImGui.Text('MONMOUTH DID');
  ImGui.Text('did:monmouth:709...17dc79C8');
  ImGui.Text('@monmouth_trader');

  ImGui.SeparatorText('Order Ticket');
  ImGui.Combo('Type', state.orderType, ORDER_TYPES_IMGUI);
  ImGui.Combo('Side', state.side, SIDES_IMGUI);
  ImGui.SliderFloat('Leverage', state.leverage, 1, 25, '%.0fx');
  ImGui.SliderFloat('Amount (ETH)', state.amount, 0.01, 5, '%.3f');

  ImGui.BeginDisabled(state.orderType[0] !== 1);
  ImGui.InputDouble('Limit Price', state.limitPrice, 1, 10, '%.2f');
  ImGui.EndDisabled();

  const notional = state.amount[0] * state.markPrice;
  ImGui.Text(`Order Value ${formatUsd(notional)}`);
  ImGui.TextColored(DIM, `Est. Fee ${(notional * 0.0004).toFixed(2)} USD`);
  ImGui.TextColored(
    DIM,
    `Slippage ${((state.spread / Math.max(state.markPrice, 1)) * 100).toFixed(3)}%`,
  );

  if (ImGui.Button('Submit Order', new ImVec2(0, 0))) {
    pushEvent(
      state,
      `${SIDES[state.side[0]]} ${state.amount[0].toFixed(3)} ${PAIRS[state.market[0]]} @ ${formatUsd(state.orderType[0] === 1 ? state.limitPrice[0] : state.markPrice)}`,
      'success',
    );
  }
  ImGui.SameLine();
  if (ImGui.Button('Cancel All')) {
    pushEvent(state, 'Canceled all local pending orders', 'warn');
  }

  ImGui.SeparatorText('Guardrails');
  ImGui.Text(`Max/Transaction ${state.maxPerTrade[0].toFixed(3)} ETH`);
  ImGui.Text(`Daily Cap ${state.dailyCap[0].toFixed(2)} ETH`);
  ImGui.Text(`Escrow-backed payments x402`);

  ImGui.SeparatorText('Metrics');
  ImGui.Text(`Render FPS ${frameRate.toFixed(1)}`);
  ImGui.Text(`Cycles Completed ${state.cycles}`);
  ImGui.Text(`Escrow Hit Rate 92%`);
  ImGui.Text(`Avg Response ${state.responseMs.toFixed(0)}ms`);
  ImGui.End();
};

const render = (
  state: AppState,
  canvasWidth: number,
  canvasHeight: number,
  frameRate: number,
): void => {
  if (canvasWidth <= 0 || canvasHeight <= 0) {
    return;
  }

  renderHeaderBar(state);

  const x = 8;
  const y = 44;
  const w = Math.max(320, canvasWidth - 16);
  const h = Math.max(240, canvasHeight - y - 8);
  const gap = 8;

  const rightWidth = Math.max(340, w * 0.25);
  const centerWidth = Math.max(280, w * 0.22);
  const leftWidth = Math.max(480, w - rightWidth - centerWidth - gap * 2);
  const upperLeftHeight = Math.max(260, h * 0.66);
  const lowerLeftHeight = h - upperLeftHeight - gap;
  const centerTopHeight = Math.max(260, h * 0.5);
  const centerBottomHeight = h - centerTopHeight - gap;

  renderMarketWindow(state, x, y, leftWidth, upperLeftHeight);
  renderActivityWindow(state, x, y + upperLeftHeight + gap, leftWidth, lowerLeftHeight);
  renderTraderAgentWindow(state, x + leftWidth + gap, y, centerWidth, centerTopHeight);
  renderResearchWindow(
    state,
    x + leftWidth + gap,
    y + centerTopHeight + gap,
    centerWidth,
    centerBottomHeight,
  );
  renderSidecarWindow(state, x + leftWidth + centerWidth + gap * 2, y, rightWidth, h, frameRate);
};

type ImGuiBackend = 'webgl' | 'webgl2';

const setBootStatus = (message: string): void => {
  let status = document.getElementById('boot-status');
  if (!status) {
    status = document.createElement('div');
    status.id = 'boot-status';
    status.style.position = 'fixed';
    status.style.left = '12px';
    status.style.bottom = '12px';
    status.style.zIndex = '20';
    status.style.padding = '6px 10px';
    status.style.borderRadius = '8px';
    status.style.fontFamily = 'ui-monospace, SFMono-Regular, Menlo, monospace';
    status.style.fontSize = '12px';
    status.style.color = '#d0d7f0';
    status.style.background = 'rgba(8, 12, 18, 0.85)';
    status.style.border = '1px solid rgba(78, 92, 130, 0.55)';
    document.body.append(status);
  }
  status.textContent = message;
};

const hideBootStatus = (): void => {
  const status = document.getElementById('boot-status');
  if (status) {
    status.remove();
  }
};

const initBackend = async (canvas: HTMLCanvasElement): Promise<ImGuiBackend> => {
  const candidates: ImGuiBackend[] = ['webgl2', 'webgl'];
  const failures: string[] = [];

  for (const backend of candidates) {
    try {
      setBootStatus(`MMPerp: initializing ${backend}...`);
      await ImGuiImplWeb.Init({ backend, canvas });
      setBootStatus(`MMPerp: ${backend} ready`);
      return backend;
    } catch (error: unknown) {
      const reason = error instanceof Error ? error.message : String(error);
      failures.push(`${backend}: ${reason}`);
    }
  }

  throw new Error(`No compatible backend found (${failures.join(' | ')})`);
};

const run = async (): Promise<void> => {
  setBootStatus('MMPerp: booting...');
  const canvas = document.getElementById('render-canvas');
  if (!(canvas instanceof HTMLCanvasElement)) {
    throw new Error('render canvas was not found');
  }

  const backend = await initBackend(canvas);
  applyTheme();
  setBootStatus(`MMPerp: ${backend} renderer active`);
  setTimeout(hideBootStatus, 1400);

  const state = initState();
  let previous = performance.now();

  const frame = (): void => {
    canvas.width = canvas.clientWidth;
    canvas.height = canvas.clientHeight;

    const now = performance.now();
    const delta = Math.min((now - previous) / 1000, 0.1);
    const frameRate = delta > 0 ? 1 / delta : 0;
    previous = now;
    updateState(state, delta);

    ImGuiImplWeb.BeginRender();
    render(state, canvas.width, canvas.height, frameRate);
    ImGuiImplWeb.EndRender();
    requestAnimationFrame(frame);
  };

  requestAnimationFrame(frame);
};

run().catch((error: unknown) => {
  setBootStatus(`MMPerp boot failed: ${error instanceof Error ? error.message : String(error)}`);
  // eslint-disable-next-line no-console
  console.error('Failed to initialize MMPerp web UI', error);
});
