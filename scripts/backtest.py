from opentrade import *
import os
'''
import opensim

tz = 'US/Eastern'
opentick = '127.0.0.1/opentrade'
database = 'test:test@iraut@127.0.0.1:5432/opentrade'
ctx, data = init(vars())
'''

os.environ['USED_SYMBOLS'] = 'US MSFT'
sec = get_exchange('US').get_security('MSFT')


def on_start(self):
  '''
  df = data.history(ctx.symbol('MSFT US'), table='_adj_', period='6000d')
  df = df.iloc[::-1].cumprod().iloc[::-1]
  series = [df.iloc[i] for i in range(len(df))]
  sec.set_adj([(int(x.name.strftime('%Y%m%d')), x.px, x.vol) for x in series])
  '''
  # sec.set_adj(((20171017, 0.25, 4), (20171020, 0.5, 2))) # forward adj
  log_info('backtest started')


def on_stop(self):
  log_info('backtest done')


def on_start_of_day(self, date):
  log_info(date, 'started', get_datetime())
  st = SecurityTuple()
  st.sec = sec
  self.start_algo('AlphaExample', {'Security': st})
  self.acc = st.acc


def on_end_of_day(self, date):
  for sec, p in self.acc.positions:
    print(sec, p)
    print(sec.md.close)
  log_info(date, 'done', get_datetime())
