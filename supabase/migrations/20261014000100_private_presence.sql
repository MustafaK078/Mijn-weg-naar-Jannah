create table if not exists public.user_presence (
  user_id uuid primary key references auth.users (id) on delete cascade,
  last_seen_at timestamptz not null default now()
);

alter table public.user_presence enable row level security;

revoke all on table public.user_presence from anon, authenticated;
grant select, insert, update, delete on table public.user_presence to authenticated;
grant select, insert, update, delete on table public.user_presence to service_role;

drop policy if exists "Users can read their own presence" on public.user_presence;
create policy "Users can read their own presence"
  on public.user_presence
  for select
  to authenticated
  using ((select auth.uid()) = user_id);

drop policy if exists "Users can register their own presence" on public.user_presence;
create policy "Users can register their own presence"
  on public.user_presence
  for insert
  to authenticated
  with check ((select auth.uid()) = user_id);

drop policy if exists "Users can update their own presence" on public.user_presence;
create policy "Users can update their own presence"
  on public.user_presence
  for update
  to authenticated
  using ((select auth.uid()) = user_id)
  with check ((select auth.uid()) = user_id);

drop policy if exists "Users can delete their own presence" on public.user_presence;
create policy "Users can delete their own presence"
  on public.user_presence
  for delete
  to authenticated
  using ((select auth.uid()) = user_id);

create table if not exists public.app_admins (
  user_id uuid primary key references auth.users (id) on delete cascade,
  created_at timestamptz not null default now()
);

alter table public.app_admins enable row level security;
revoke all on table public.app_admins from anon, authenticated;
grant select on table public.app_admins to service_role;
