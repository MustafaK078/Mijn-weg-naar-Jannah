import { createClient } from "npm:@supabase/supabase-js@2";

const corsHeaders = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Headers": "authorization, apikey, content-type, x-client-info",
  "Access-Control-Allow-Methods": "GET, OPTIONS",
};

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { ...corsHeaders, "Content-Type": "application/json" },
  });
}

Deno.serve(async (request: Request) => {
  if (request.method === "OPTIONS") {
    return new Response("ok", { headers: corsHeaders });
  }
  if (request.method !== "GET" && request.method !== "POST") {
    return jsonResponse({ error: "Method not allowed" }, 405);
  }

  const authorization = request.headers.get("Authorization");
  const token = authorization?.match(/^Bearer\s+(.+)$/i)?.[1];
  if (!token) {
    return jsonResponse({ error: "Authentication required" }, 401);
  }

  const supabaseUrl = Deno.env.get("SUPABASE_URL");
  const anonKey = Deno.env.get("SUPABASE_ANON_KEY");
  const serviceRoleKey = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY");
  if (!supabaseUrl || !anonKey || !serviceRoleKey) {
    console.error("Required Supabase function secrets are missing");
    return jsonResponse({ error: "Server configuration is incomplete" }, 500);
  }

  const authClient = createClient(supabaseUrl, anonKey, {
    auth: { persistSession: false, autoRefreshToken: false },
  });
  const { data: authData, error: authError } = await authClient.auth.getUser(token);
  if (authError || !authData.user) {
    return jsonResponse({ error: "Invalid session" }, 401);
  }

  const adminClient = createClient(supabaseUrl, serviceRoleKey, {
    auth: { persistSession: false, autoRefreshToken: false },
  });
  const { data: adminRecord, error: adminError } = await adminClient
    .from("app_admins")
    .select("user_id")
    .eq("user_id", authData.user.id)
    .maybeSingle();

  if (adminError) {
    console.error("Could not verify administrator role", adminError.message);
    return jsonResponse({ error: "Administrator role could not be verified" }, 500);
  }
  if (!adminRecord) {
    return jsonResponse({ error: "Administrator access required" }, 403);
  }

  const cutoff = Date.now() - 5 * 60 * 1000;
  const onlineCutoff = Date.now() - 90 * 1000;
  const activeAccounts: Array<{
    email: string;
    online: boolean;
    last_seen_at: string;
  }> = [];

  for (let page = 1; ; page++) {
    const { data: usersData, error: usersError } = await adminClient.auth.admin.listUsers({
      page,
      perPage: 1000,
    });
    if (usersError) {
      console.error("Could not list authenticated accounts", usersError.message);
      return jsonResponse({ error: "Active accounts could not be loaded" }, 500);
    }

    const users = usersData.users;
    if (users.length === 0) break;
    const userIds = users.map((user) => user.id);
    const { data: presences, error: presenceError } = await adminClient
      .from("user_presence")
      .select("user_id,last_seen_at")
      .in("user_id", userIds);

    if (presenceError) {
      console.error("Could not load account presence", presenceError.message);
      return jsonResponse({ error: "Account presence could not be loaded" }, 500);
    }

    const lastSeenByUser = new Map(
      (presences ?? []).map((presence) => [presence.user_id, presence.last_seen_at]),
    );
    for (const user of users) {
      const lastSeen = lastSeenByUser.get(user.id);
      const lastSeenTime = lastSeen ? Date.parse(lastSeen) : NaN;
      if (!user.email || !Number.isFinite(lastSeenTime) || lastSeenTime < cutoff) continue;
      activeAccounts.push({
        email: user.email,
        online: lastSeenTime >= onlineCutoff,
        last_seen_at: lastSeen as string,
      });
    }

    if (users.length < 1000) break;
  }

  activeAccounts.sort((a, b) => b.last_seen_at.localeCompare(a.last_seen_at));
  return jsonResponse({ accounts: activeAccounts });
});
